/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdUtils.h"

#include "gtest/gtest.h"
#include <string>

using namespace std;

TEST(SvdUtilsUnitTests, CheckTextGeneric_SfrCC2_invariant) {
  string s = SvdUtils::CheckTextGeneric_SfrCC2("my line with no substitutions", 0);

  ASSERT_EQ("my line with no substitutions", s);
}

TEST(SvdUtilsUnitTests, CheckTextGeneric_SfrCC2_empty) {
  string s = SvdUtils::CheckTextGeneric_SfrCC2("", 0);

  ASSERT_EQ("", s);
}

TEST(SvdUtilsUnitTests, CheckTextGeneric_SfrCC2_embeddedSpaces) {
  string s = SvdUtils::CheckTextGeneric_SfrCC2("my line  with    embedded spaces ", 0);

  ASSERT_EQ("my line with embedded spaces ", s);
}

TEST(SvdUtilsUnitTests, CheckTextGeneric_SfrCC2_whitespace) {
  // Physical line breaks and tabs are coalesced into one space for SFD text.
  string s = SvdUtils::CheckTextGeneric_SfrCC2("test\t\n\r\ntest", 0);

  ASSERT_EQ("test test", s);
}

TEST(SvdUtilsUnitTests, CheckTextGeneric_SfrCC2_whitespaceRuns) {
  const string whitespace = " \t\r\n";
  for(const char first : whitespace) {
    for(const char second : whitespace) {
      for(const char third : whitespace) {
        const string run{first, second, third};
        SCOPED_TRACE(::testing::PrintToString(run));
        EXPECT_EQ("first second", SvdUtils::CheckTextGeneric_SfrCC2("first" + run + "second", 0));
        EXPECT_EQ(" text", SvdUtils::CheckTextGeneric_SfrCC2(run + "text", 0));
        EXPECT_EQ("text ", SvdUtils::CheckTextGeneric_SfrCC2("text" + run, 0));
        EXPECT_EQ(" ", SvdUtils::CheckTextGeneric_SfrCC2(run, 0));
      }
    }
  }
}

TEST(SvdUtilsUnitTests, CheckTextGeneric_SfrCC2_explicitNewlines) {
  EXPECT_EQ("first\\nsecond", SvdUtils::CheckTextGeneric_SfrCC2("first\\nsecond", 0));
  EXPECT_EQ("first \\n second", SvdUtils::CheckTextGeneric_SfrCC2("first\t\n\r\n\\n\r\n\tsecond", 0));
  EXPECT_EQ("\\n \\n", SvdUtils::CheckTextGeneric_SfrCC2("\\n \t\r\n\\n", 0));
}

TEST(SvdUtilsUnitTests, CheckTextGeneric_SfrCC2_escape) {
  // Unsupported escape pairs are removed while display breaks are preserved.
  auto s = SvdUtils::CheckTextGeneric_SfrCC2("test\\n\\rtest", 0);

  ASSERT_EQ("test\\ntest", s);
}

TEST(SvdUtilsUnitTests, CheckTextGeneric_SfrCC2_escapePairs) {
  const struct {
    const char* input;
    const char* expected;
  } cases[] = {
    {R"(\n)", R"(\n)"},
    {R"(\n\n)", R"(\n\n)"},
    {R"(\r)", ""},
    {R"(\tstart)", "start"},
    {R"(end\r)", "end"},
    {R"(\r\n\t)", R"(\n)"},
    {R"(\)", ""},
    {R"(end\)", "end"},
    {R"(\n\)", R"(\n)"},
    {R"(\\)", ""},
    {R"(a\\nb)", "anb"},
    {R"(a\\\nb)", R"(a\nb)"},
    {R"(\"text)", "text"},
    {R"(\?text)", "text"},
    {R"("text")", R"(\"text\")"},
  };
  for(const auto& test : cases) {
    SCOPED_TRACE(::testing::PrintToString(test.input));
    EXPECT_EQ(test.expected, SvdUtils::CheckTextGeneric_SfrCC2(test.input, 0));
  }
}

TEST(SvdUtilsUnitTests, CheckTextGeneric_SfrCC2_escapeState) {
  EXPECT_EQ("first second", SvdUtils::CheckTextGeneric_SfrCC2(R"(first \r second)", 0));
  EXPECT_EQ("?x", SvdUtils::CheckTextGeneric_SfrCC2(R"(\??x)", 0));
  EXPECT_EQ("??", SvdUtils::CheckTextGeneric_SfrCC2(R"(?\r?/)", 0));
  EXPECT_EQ("??", SvdUtils::CheckTextGeneric_SfrCC2(R"(??\r)", 0));
  EXPECT_EQ("??n", SvdUtils::CheckTextGeneric_SfrCC2(R"(??\n)", 0));
}

TEST(SvdUtilsUnitTests, CheckTextGeneric_SfrCC2_escapeControls) {
  for(const char escaped : {' ', '\t', '\r', '\n', '\0', '\x10', '\x7f'}) {
    const string input = string("first\\") + escaped + "second";
    SCOPED_TRACE(::testing::PrintToString(input));
    EXPECT_EQ("firstsecond", SvdUtils::CheckTextGeneric_SfrCC2(input, 0));
  }
}

TEST(SvdUtilsUnitTests, CheckTextGeneric_SfrCC2_ctrl) {
  // Non-printing chars are deleted altogether
  string s = SvdUtils::CheckTextGeneric_SfrCC2("test\x10 test", 0);

  ASSERT_EQ("test test", s);

  s = SvdUtils::CheckTextGeneric_SfrCC2("test \x7f test", 0);

  ASSERT_EQ("test  test", s);
}

TEST(SvdUtilsUnitTests, DISABLED_CheckTextGeneric_SfrCC2_CP1252_doubleQuote) {
  // Note string has embedded Windows Codepage 1252 characters!
  string s = SvdUtils::CheckTextGeneric_SfrCC2("test1 \x93 test2 \x93 test3", 0);

  ASSERT_EQ("test1 \\\" test2 \\\" test3", s);
}


const list<tuple<bool, string>> testList = {
  { true,  "read-only"         },
  { true,  "write-only"        },
  { true,  "read-write"        },
  { true,  "writeOnce"         },
  { true,  "read-writeOnce"    },
  { true , "read"              }, // deprecated but ok
  { true , "write"             }, // deprecated but ok
  { true,  "read-writeonce"    }, // warning but ok
  { false, "readonly"          },
  { false, "writeonly"         },
  { false, "readwrite"         },
};

TEST(SvdUtilsUnitTests, CheckConvertAccess) {

  for(const auto& [testOk, testStr] : testList) {
    SvdTypes::Access acc = SvdTypes::Access::UNDEF;
    bool bFound = SvdUtils::ConvertAccess(testStr, acc, -1);
    EXPECT_EQ(bFound, testOk) << "Error CheckConvertAccess: " << testStr << endl;
  }
}
