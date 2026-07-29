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
  // Whitespace chars are coalesced
  string s = SvdUtils::CheckTextGeneric_SfrCC2("test\t\n\r\ntest", 0);

  // FIXME: is it correct that there should be two spaces here?
  ASSERT_EQ("test  test", s);
}

TEST(SvdUtilsUnitTests, DISABLED_CheckTextGeneric_SfrCC2_escape) {
  // An escape char other than \\n is removed
  auto s = SvdUtils::CheckTextGeneric_SfrCC2("test\\n\\rtest", 0);

  ASSERT_EQ("test\\ntest", s); // FIXME: current result is "test\\n\\test"
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
