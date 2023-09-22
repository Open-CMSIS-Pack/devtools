/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "RteUtils.h"

#include "gtest/gtest.h"

using namespace std;

TEST(VersionCmpTest, VersionMatchMode) {

  EXPECT_EQ(VersionCmp::MatchModeFromString("enforced"), VersionCmp::MatchMode::ENFORCED_VERSION);
  EXPECT_EQ(VersionCmp::MatchModeFromString("fixed"),    VersionCmp::MatchMode::FIXED_VERSION);
  EXPECT_EQ(VersionCmp::MatchModeFromString("latest"),   VersionCmp::MatchMode::LATEST_VERSION);
  EXPECT_EQ(VersionCmp::MatchModeFromString("excluded"), VersionCmp::MatchMode::EXCLUDED_VERSION);

  EXPECT_EQ(VersionCmp::MatchModeToString(VersionCmp::MatchMode::ENFORCED_VERSION), "enforced");
  EXPECT_EQ(VersionCmp::MatchModeToString(VersionCmp::MatchMode::FIXED_VERSION),"fixed");
  EXPECT_EQ(VersionCmp::MatchModeToString(VersionCmp::MatchMode::LATEST_VERSION), "latest");
  EXPECT_EQ(VersionCmp::MatchModeToString(VersionCmp::MatchMode::EXCLUDED_VERSION), "excluded");
}

TEST(VersionCmpTest, CeilFloor) {

  EXPECT_EQ(VersionCmp::Ceil("2.3.0"), "3.0.0-");
  EXPECT_EQ(VersionCmp::Ceil("2.3.0", false), "3.0.0");
  EXPECT_EQ(VersionCmp::Ceil(""), "1.0.0-");
  EXPECT_EQ(VersionCmp::Ceil("a"), "1.0.0-");

  EXPECT_EQ(VersionCmp::Floor("2.3.0"), "2.0.0");
  EXPECT_EQ(VersionCmp::Floor(""), ".0.0");
}


TEST(VersionCmpTest, VersionCompare) {
  EXPECT_EQ( -1, VersionCmp::Compare("6.5.0-a", "6.5.0", true));
  EXPECT_EQ(  0, VersionCmp::Compare("6.5.0-a", "6.5.0-A", true));
  EXPECT_EQ(  0, VersionCmp::Compare("6.5.0+b", "6.5.0+A", true));
  EXPECT_EQ( -1, VersionCmp::Compare("6.5.0-a", "6.5.0-B", true));
  EXPECT_EQ(  0, VersionCmp::Compare("6.5.0-a", "6.5.0-A", false));
  EXPECT_EQ(  0, VersionCmp::Compare("6.5.0", "6.5.0", false));
  EXPECT_EQ( -1, VersionCmp::Compare("6.5.0", "6.5.1"));
  EXPECT_EQ( -2, VersionCmp::Compare("6.4.0", "6.5.0"));
  EXPECT_EQ( -3, VersionCmp::Compare("2.5.0", "6.5.0"));
  EXPECT_EQ(  1, VersionCmp::Compare("6.5.9", "6.5.1"));
  EXPECT_EQ(  2, VersionCmp::Compare("6.6.0", "6.5.0"));
  EXPECT_EQ(  3, VersionCmp::Compare("7.5.0", "6.5.0"));
  EXPECT_EQ(-1, VersionCmp::Compare("6.5.0-", "6.5.0-a", true));
  EXPECT_EQ(0, VersionCmp::Compare("6.5.0-a+b", "6.5.0-a+B", true));

  EXPECT_EQ(VersionCmp::RemoveVersionMeta("1.2.3-a+b"), "1.2.3-a");
  EXPECT_EQ(VersionCmp::RemoveVersionMeta("1.2.3+b"), "1.2.3");

  /*Ideally It should fail as the input
  given is not compliant to Semantic versioning*/
  EXPECT_EQ(  0, VersionCmp::Compare("1.2.5.0.1.2", "1.2.5.0.1.2"));
  EXPECT_EQ(  3, VersionCmp::Compare("Test", "1.2.5.0"));
  EXPECT_EQ( -3, VersionCmp::Compare("1.2.3", "v1.2.3"));
}

TEST(VersionCmpTest, VersionRangeCompare) {
  EXPECT_EQ(0, VersionCmp::RangeCompare("3.2.0", "3.1.0:3.8.0"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("3.2.0", "3.1.0"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("3.2.0", ":3.8.0"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("3.2.0", "3.2.0"));
  EXPECT_EQ(1, VersionCmp::RangeCompare("3.2.0", ":3.2.0-"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("3.0.0", "2.9.0"));
  /* Major version is greater that allowed by semantic version */
  EXPECT_EQ(3, VersionCmp::CompatibleRangeCompare("3.0.0", "2.9.0"));

  EXPECT_EQ(0, VersionCmp::RangeCompare("1.0.0", "1.0.0:2.0.0"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("2.0.0", "1.0.0:2.0.0"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("1.99.99", "1.0.0:2.0.0"));
  EXPECT_EQ(1, VersionCmp::RangeCompare("1.99.99", "1.0.0:1.99.9"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("1.0.0", "1.0.0:2.0.0-"));
  EXPECT_EQ(1, VersionCmp::RangeCompare("2.0.0", "1.0.0:2.0.0-"));
  EXPECT_EQ(1, VersionCmp::RangeCompare("2.0.0-a", "1.0.0:2.0.0-"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("2.0.0-a", "2.0.0-:2.0.0"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("1.99.99", "1.0.0:2.0.0-"));

  EXPECT_EQ( 3, VersionCmp::RangeCompare("9.0.0", "1.0.0:2.0.0"));
  EXPECT_EQ(-3, VersionCmp::RangeCompare("0.9.0", "1.0.0:2.0.0"));

  /* Greater than max version : Patch version out of range */
  EXPECT_EQ(  1, VersionCmp::RangeCompare("3.8.2", "3.1.0:3.8.0"));

  /* Greater than max version : Minor version out of range */
  EXPECT_EQ(  2, VersionCmp::RangeCompare("3.9.0", "3.1.0:3.8.0"));

  /* Greater than max version : Major version out of range */
  EXPECT_EQ(  3, VersionCmp::RangeCompare("4.2.0", "3.1.0:3.8.0"));

  /* Version matches */
  EXPECT_EQ(  0, VersionCmp::RangeCompare("3.1.0", "3.1.0:3.1.0"));

  /* less than Min version : Patch version out of range */
  EXPECT_EQ( -1, VersionCmp::RangeCompare("3.3.8", "3.3.9:3.3.9"));

  /* less than Min version : Minor version out of range */
  EXPECT_EQ( -2, VersionCmp::RangeCompare("3.2.9", "3.3.9:3.3.9"));

  /* less than Min version : Major version out of range */
  EXPECT_EQ( -3, VersionCmp::RangeCompare("2.3.9", "3.3.9:3.3.9"));
}

TEST(VersionCmpTest, MatchModeFromVersionString) {
  map<string, VersionCmp::MatchMode> inputs{
    {"Vendor",               VersionCmp::MatchMode::LATEST_VERSION},
    {"Vendor::Name@1.2.3",   VersionCmp::MatchMode::FIXED_VERSION},
    {"Vendor::Name@>=1.2.3", VersionCmp::MatchMode::HIGHER_OR_EQUAL},
    {"Vendor@Test",          VersionCmp::MatchMode::LATEST_VERSION},
  };

  for (auto& [input, expectedOutput] : inputs) {
    EXPECT_EQ(expectedOutput, VersionCmp::MatchModeFromVersionString(input));
  }
}

TEST(VersionCmpTest, GetMatchingVersion) {
  map<pair<string, set<string>>, string> inputs{
    // <versionfilter, <>>
    {{"",             {"1.1.1", "2.2.2", "3.3.3"}},  "3.3.3"},
    {{"@2.2.2",       {"1.1.1", "2.2.2", "3.3.3"}},  "2.2.2"},
    {{"@>=1.5.5",     {"1.1.1", "2.2.2", "3.3.3"}},  "3.3.3"},
    {{"@>=3.3.3",     {"1.1.1", "2.2.2", "3.3.3"}},  "3.3.3"},
    {{"@1.2.3",       {"1.1.1", "2.2.2", "3.3.3"}},  ""},

    // version range
    {{"3.6.0",        {"3.5.0"}},                    ""},
    {{"3.2.0",        {"3.2.0"}},                    "3.2.0"},
    {{"3.2.0",        {"1.1.0", "2.8.0","3.5.0"}},   "3.5.0"},
    {{":3.2.0",       {"1.1.0", "2.8.0","3.5.0"}},   "2.8.0"},
    {{"1.2.0:3.2.0",  {"1.1.0", "1.2.0","3.5.0"}},   "1.2.0"},
    {{"1.0.0:1.99.9", {"1.99.99", "2.2.0","3.5.0"}}, ""},
    {{"1.2.0:3.2.0",  {"1.1.0", "1.2.0", "1.2.5", "3.2.0", "3.5.0"}}, "3.2.0"},
  };

  for (auto& [input, expectedOutput] : inputs) {
    EXPECT_EQ(expectedOutput, VersionCmp::GetMatchingVersion(input.first, input.second)) <<
      "error: for input " << input.first << " expected output is \"" << expectedOutput << "\"" << endl;
  }
  EXPECT_EQ("4.0.1", VersionCmp::GetMatchingVersion("3.6.0", { "4.0.0", "4.0.1" }));
  EXPECT_EQ("", VersionCmp::GetMatchingVersion("3.6.0", { "4.0.0", "4.0.1" }, true));

}
TEST(VersionCmpTest, ComparatorBase) {
  VersionCmp::ComparatorBase comparator;
  EXPECT_EQ(-2, comparator.Compare("1.1.0", "1.2.0"));
  VersionCmp::ComparatorBase comparator1('@');
  EXPECT_EQ(-2, comparator1.Compare("Test@1.1.0", "Test@1.2.0"));
  EXPECT_EQ(1, comparator1.Compare("Foo@1.1.0", "Bar@1.2.0"));
}
// end of VersionCmpTest.cpp
