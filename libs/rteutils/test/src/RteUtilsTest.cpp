/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "RteUtils.h"

#include "gtest/gtest.h"

using namespace std;


static void CheckVendorMatch(const vector<string>& vendors, bool expect) {
  for (auto iter = vendors.cbegin(); iter != vendors.cend() - 1; iter++) {
    EXPECT_EQ(expect, DeviceVendor::Match(*iter, *(iter + 1))) << "Failed to compare " << (*iter) << " & " << *(iter + 1);
  }
}

TEST(RteUtilsTest, EqualNoCase) {

  EXPECT_TRUE(RteUtils::EqualNoCase("", ""));

  EXPECT_TRUE(RteUtils::EqualNoCase("SameCase", "SameCase"));
  EXPECT_TRUE(RteUtils::EqualNoCase("UPPER_CASE", "UPPER_CASE"));
  EXPECT_TRUE(RteUtils::EqualNoCase("lower case", "lower case"));
  EXPECT_TRUE(RteUtils::EqualNoCase("Mixed_Case", "mIXED_cASE"));

  EXPECT_FALSE(RteUtils::EqualNoCase("Mixed_Case1", "Mixed_Case01"));
  EXPECT_FALSE(RteUtils::EqualNoCase("Mixed_Case", "Mixed_Cake"));
}

TEST(RteUtilsTest, GetPrefix) {

  EXPECT_EQ(RteUtils::GetPrefix("prefix:suffix"), "prefix");
  EXPECT_EQ(RteUtils::GetPrefix("prefix:suffix", '-'),"prefix:suffix");
  EXPECT_EQ(RteUtils::GetPrefix("prefix:suffix",':', true) , "prefix:");

  EXPECT_EQ( RteUtils::GetPrefix("prefix-suffix"), "prefix-suffix");
  EXPECT_EQ( RteUtils::GetPrefix("prefix-suffix", '-'),"prefix");

}

TEST(RteUtilsTest, GetSuffix) {

  EXPECT_EQ(RteUtils::GetSuffix("prefix:suffix"), "suffix");
  EXPECT_TRUE(RteUtils::GetSuffix("prefix:suffix", '-').empty());
  EXPECT_EQ(RteUtils::GetSuffix("prefix:suffix", ':', true), ":suffix");

  EXPECT_TRUE(RteUtils::GetSuffix("prefix-suffix").empty());
  EXPECT_EQ(RteUtils::GetSuffix("prefix-suffix", '-'), "suffix");
}

TEST(RteUtilsTest, ExtractFileExtension) {

  EXPECT_TRUE(RteUtils::ExtractFileExtension("myfile").empty());
  EXPECT_EQ(RteUtils::ExtractFileExtension("my.dir/my.file"), "file");
  EXPECT_EQ(RteUtils::ExtractFileExtension("my.dir/my.file.ext"), "ext");
  EXPECT_EQ(RteUtils::ExtractFileExtension("my.dir/my.file.ext", true), ".ext");
}

TEST(RteUtilsTest, ExtractFileName) {

  EXPECT_EQ(RteUtils::ExtractFileName("myfile.ext"), "myfile.ext");
  EXPECT_EQ(RteUtils::ExtractFileName("./my.dir/myfile"), "myfile");
  EXPECT_EQ(RteUtils::ExtractFileName("./my.dir/my.file"), "my.file");
  EXPECT_EQ(RteUtils::ExtractFileName("./my.dir/my.file.ext"), "my.file.ext");

  EXPECT_EQ(RteUtils::ExtractFileName("..\\my.dir\\myfile"), "myfile");
  EXPECT_EQ(RteUtils::ExtractFileName("..\\my.dir\\my.file"), "my.file");
  EXPECT_EQ(RteUtils::ExtractFileName("..\\my.dir\\my.file.ext"), "my.file.ext");

}

TEST(RteUtilsTest, ExtractFileBaseName) {

  EXPECT_EQ(RteUtils::ExtractFileBaseName("myfile.ext"), "myfile");

  EXPECT_EQ(RteUtils::ExtractFileBaseName("my.dir/myfile"), "myfile");
  EXPECT_EQ(RteUtils::ExtractFileBaseName("my.dir/my.file"), "my");
  EXPECT_EQ(RteUtils::ExtractFileBaseName("my.dir/my.file.ext"), "my.file");

  EXPECT_EQ(RteUtils::ExtractFileBaseName("my.dir\\myfile"), "myfile");
  EXPECT_EQ(RteUtils::ExtractFileBaseName("my.dir\\my.file"), "my");
  EXPECT_EQ(RteUtils::ExtractFileBaseName("my.file.ext"), "my.file");

}

TEST(RteUtilsTest, ExpandInstancePlaceholders) {

  static string text =
    "#define USBD%Instance%_PORT %Instance%\n"
    "#define USBD%Instance%_HS 0";

  static string text1 =
    "#define USBD0_PORT 0\n"
    "#define USBD0_HS 0\n";

  static string text2 =
    "#define USBD0_PORT 0\n"
    "#define USBD0_HS 0\n"
    "#define USBD1_PORT 1\n"
    "#define USBD1_HS 0\n";

  string expanded = RteUtils::ExpandInstancePlaceholders(text, 1);
  EXPECT_EQ(expanded, text1);

  expanded = RteUtils::ExpandInstancePlaceholders(text, 2);
  EXPECT_EQ(expanded, text2);
}

TEST(RteUtilsTest, VersionMatchMode) {

  EXPECT_EQ(VersionCmp::MatchModeFromString(""), VersionCmp::ANY_VERSION);
  EXPECT_EQ(VersionCmp::MatchModeFromString("fixed"), VersionCmp::VersionCmp::FIXED_VERSION);
  EXPECT_EQ(VersionCmp::MatchModeFromString("latest"), VersionCmp::VersionCmp::LATEST_VERSION);
  EXPECT_EQ(VersionCmp::MatchModeFromString("excluded"), VersionCmp::VersionCmp::EXCLUDED_VERSION);

  EXPECT_EQ(VersionCmp::MatchModeToString(VersionCmp::ANY_VERSION).empty(), true);
  EXPECT_EQ(VersionCmp::MatchModeToString(VersionCmp::VersionCmp::FIXED_VERSION),"fixed");
  EXPECT_EQ(VersionCmp::MatchModeToString(VersionCmp::VersionCmp::LATEST_VERSION), "latest");
  EXPECT_EQ(VersionCmp::MatchModeToString(VersionCmp::VersionCmp::EXCLUDED_VERSION), "excluded");
}

TEST(RteUtilsTest, VersionCompare) {
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

  /*Ideally It should fail as the input
  given is not compliant to Semantic versioning*/
  EXPECT_EQ(  0, VersionCmp::Compare("1.2.5.0.1.2", "1.2.5.0.1.2"));
  EXPECT_EQ(  3, VersionCmp::Compare("Test", "1.2.5.0"));
  EXPECT_EQ( -3, VersionCmp::Compare("1.2.3", "v1.2.3"));
}

TEST(RteUtilsTest, VersionRangeCompare) {
  EXPECT_EQ(0, VersionCmp::RangeCompare("3.2.0", "3.1.0:3.8.0"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("3.2.0", "3.1.0"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("3.2.0", ":3.8.0"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("3.2.0", "3.2.0"));
  EXPECT_EQ(1, VersionCmp::RangeCompare("3.2.0", ":3.2.0-"));

  EXPECT_EQ(0, VersionCmp::RangeCompare("1.0.0", "1.0.0:2.0.0"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("2.0.0", "1.0.0:2.0.0"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("1.99.99", "1.0.0:2.0.0"));
  EXPECT_EQ(1, VersionCmp::RangeCompare("1.99.99", "1.0.0:1.99.9"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("1.0.0", "1.0.0:2.0.0-"));
  EXPECT_EQ(1, VersionCmp::RangeCompare("2.0.0", "1.0.0:2.0.0-"));
  EXPECT_EQ(1, VersionCmp::RangeCompare("2.0.0-a", "1.0.0:2.0.0-"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("2.0.0-a", "2.0.0-:2.0.0"));
  EXPECT_EQ(0, VersionCmp::RangeCompare("1.99.99", "1.0.0:2.0.0-"));

  EXPECT_EQ( 3,  VersionCmp::RangeCompare("9.0.0", "1.0.0:2.0.0"));
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


TEST(RteUtilsTest, WildCardsTo) {

  string test_input = "This?? is a *test1* _string-!#.";
  string regex = "This.. is a .*test1.* _string-!#\\.";
  string replaced = "Thisxx_is_a_xtest1x__string-__.";

  string converted = WildCards::ToRegEx(test_input);
  EXPECT_EQ(converted, regex);

  converted = WildCards::ToX(test_input);
  EXPECT_EQ(converted, replaced);
}

TEST(RteUtilsTest, WildCardMatch) {
  EXPECT_EQ(true, WildCards::Match("a", "a"));
  EXPECT_EQ(false, WildCards::Match("a", ""));
  EXPECT_EQ(false, WildCards::Match("", "d"));
  EXPECT_EQ(false, WildCards::Match("", "*"));
  EXPECT_EQ(true, WildCards::Match("a*", "a*d"));
  EXPECT_EQ(false, WildCards::Match("a*", "*d"));
  EXPECT_EQ(true, WildCards::Match("a*", "abcd"));
  EXPECT_EQ(false, WildCards::Match("a*", "xycd"));
  EXPECT_EQ(true, WildCards::Match("a*d", "*d"));
  EXPECT_EQ(true, WildCards::Match("a*d", "abcd"));
  EXPECT_EQ(false, WildCards::Match("a*d", "abxx"));
  EXPECT_EQ(false, WildCards::Match("a*d", "abxyz"));
  EXPECT_EQ(false, WildCards::Match("a*d", "xycd"));
  EXPECT_EQ(true, WildCards::Match("*d", "abcd"));
  EXPECT_EQ(true, WildCards::Match("*d", "d"));
  EXPECT_EQ(true, WildCards::Match("*c*", "abcd"));
  EXPECT_EQ(true, WildCards::Match("abcd", "a**d"));
  EXPECT_EQ(true, WildCards::Match("abcd", "a??d"));
  EXPECT_EQ(true, WildCards::Match("abcd", "?bc?"));
  EXPECT_EQ(true, WildCards::Match("abc?", "abc*"));
  EXPECT_EQ(true, WildCards::Match("ab?d", "ab??"));
  EXPECT_EQ(false, WildCards::Match("ab?d", "abc??"));
  EXPECT_EQ(false, WildCards::Match("?bcd", "abc??"));
  EXPECT_EQ(true, WildCards::Match("?bcd", "*bcd"));
  EXPECT_EQ(false, WildCards::Match("?bcd", "abc???"));
  EXPECT_EQ(true, WildCards::Match("abc?", "ab??"));
  EXPECT_EQ(false, WildCards::Match("abc?", "abc??"));
  EXPECT_EQ(false, WildCards::Match("ab??", "abc??"));
  EXPECT_EQ(true, WildCards::Match("abc*", "ab*?"));
  EXPECT_EQ(true, WildCards::Match("abc*", "abc?*"));
  EXPECT_EQ(true, WildCards::Match("ab*?", "abc?*"));
  EXPECT_EQ(false, WildCards::Match("abcX-1", "abcX-2"));
  EXPECT_EQ(false, WildCards::Match("abcX-1", "abcX-3"));
  EXPECT_EQ(false, WildCards::Match("abcX-1", "abcY-1"));
  EXPECT_EQ(false, WildCards::Match("abcX-1", "abcY-2"));
  EXPECT_EQ(true, WildCards::Match("abcX-1", "abc[XY]-[12]"));
  EXPECT_EQ(false, WildCards::Match("abcZ-1", "abc[XY]-[12]"));
  EXPECT_EQ(true, WildCards::Match("abcY-2", "abc[XY]-[12]"));

  EXPECT_EQ(true, WildCards::Match("Prefix_*_Suffix", "Prefix_Mid_Suffix"));
  EXPECT_EQ(true, WildCards::Match("Prefix_*_Suffix", "Prefix_Mid_V_Suffix"));
  EXPECT_EQ(true, WildCards::Match("Prefix_*_Suffix", "Prefix_Mid_Suffix_Suffix"));
  EXPECT_EQ(true, WildCards::Match("Prefix*_Suffix", "Prefix_Mid_Suffix"));
  EXPECT_EQ(true, WildCards::Match("Prefix*Suffix", "Prefix_Mid_Suffix"));
  EXPECT_EQ(true, WildCards::Match("Prefix*Suffix", "Prefix_Mid_Suffix_Suffix"));
  EXPECT_EQ(true, WildCards::Match("Prefix_*Suffix", "Prefix_Mid_Suffix"));

  EXPECT_EQ(true, WildCards::Match("Prefix.*.Suffix", "Prefix.Mid.Suffix"));

  std::vector<string> inputStr{ "STM32F10[123]?[CDE]", "STM32F103ZE", "*", "*?", "?*", "?*?", "*?*", "**", "**?" };
  for (auto iter = inputStr.cbegin(); iter != inputStr.cend() - 1; iter++) {
    EXPECT_TRUE(WildCards::Match(*iter, *(iter + 1))) << "Failed for " << (*iter) << " & " << *(iter + 1);
  }
}

TEST(RteUtilsTest, AlnumCmp_Char) {
  EXPECT_EQ( -1, AlnumCmp::Compare(nullptr, "2.1"));
  EXPECT_EQ(  1, AlnumCmp::Compare("10.1", nullptr));
  EXPECT_EQ(  0, AlnumCmp::Compare(nullptr, nullptr));
  EXPECT_EQ(  1, AlnumCmp::Compare("10.1", "2.1"));
  EXPECT_EQ(  0, AlnumCmp::Compare("2.01", "2.1")); // 01 is considered as 1
  EXPECT_EQ(  0, AlnumCmp::Compare("2.01", "2.01"));
  EXPECT_EQ( -1, AlnumCmp::Compare("2.1", "2.15"));
  EXPECT_EQ( -1, AlnumCmp::Compare("2a1", "2a5"));
  EXPECT_EQ(  1, AlnumCmp::Compare("a21", "2a5")); // ascii value comparision
  EXPECT_EQ( -1, AlnumCmp::Compare("a21", "A25", false));
}

TEST(RteUtilsTest, AlnumCmp_String) {
  EXPECT_EQ( -1, AlnumCmp::Compare(string(""), string("2.1")));
  EXPECT_EQ(  1, AlnumCmp::Compare(string("10.1"), string("")));
  EXPECT_EQ(  0, AlnumCmp::Compare(string(""), string("")));
  EXPECT_EQ(  1, AlnumCmp::Compare(string("10.1"), string("2.1")));
  EXPECT_EQ(  0, AlnumCmp::Compare(string("2.01"), string("2.1"))); // 01 is considered as 1
  EXPECT_EQ(  0, AlnumCmp::Compare(string("2.01"), string("2.01")));
  EXPECT_EQ( -1, AlnumCmp::Compare(string("2.1"), string("2.15")));
  EXPECT_EQ( -1, AlnumCmp::Compare(string("2a1"), string("2a5")));
  EXPECT_EQ(  1, AlnumCmp::Compare(string("a21"), string("2a5"))); // ascii value comparision
  EXPECT_EQ( -1, AlnumCmp::Compare(string("a21"), string("A25"), false));
}

TEST(RteUtilsTest, AlnumLenCmp) {
  EXPECT_EQ( -1, AlnumCmp::CompareLen(string(""), string("2.1")));
  EXPECT_EQ(  1, AlnumCmp::CompareLen(string("10.1"), string("")));
  EXPECT_EQ(  0, AlnumCmp::CompareLen(string(""), string("")));
  EXPECT_EQ(  1, AlnumCmp::CompareLen(string("10.1"), string("2.1")));
  EXPECT_EQ(  1, AlnumCmp::CompareLen(string("2.01"), string("2.1")));
  EXPECT_EQ(  0, AlnumCmp::CompareLen(string("2.01"), string("2.01")));
  EXPECT_EQ( -1, AlnumCmp::CompareLen(string("2.1"), string("2.15")));
  EXPECT_EQ( -1, AlnumCmp::CompareLen(string("2a1"), string("2a5")));
  EXPECT_EQ(  1, AlnumCmp::CompareLen(string("a21"), string("2a5")));
  EXPECT_EQ( -1, AlnumCmp::CompareLen(string("a21"), string("A25"), false));
}

TEST(RteUtilsTest, VendorCompare)
{
  CheckVendorMatch({ "ARM", "ARM CMSIS" }, false);
  CheckVendorMatch({ "Cypress", "Cypress:114", "Cypress:100" }, true);
  CheckVendorMatch({ "Atmel", "Atmel:3", "Microchip", "Microchip:3"}, true);
  CheckVendorMatch({ "Milandr", "Milandr:99", "milandr", "milandr:99"}, true);
  CheckVendorMatch({ "ABOV", "ABOV:126", "ABOV Semiconductor", "ABOV Semiconductor:126" }, true);
  CheckVendorMatch({ "Realtek", "Realtek:124", "Realtek Semiconductor", "Realtek Semiconductor:124"}, true);
  CheckVendorMatch({ "Texas Instruments", "Texas Instruments:16", "TI", "TI:16"}, true);

  CheckVendorMatch({ "Spansion", "Spansion:19", "Cypress", "Cypress:19",
                       "Fujitsu", "Fujitsu:19", "Fujitsu Semiconductor", "Fujitsu Semiconductor:19",
                       "Fujitsu Semiconductors", "Fujitsu Semiconductors:19" }, true);

  CheckVendorMatch({ "NXP", "NXP:11", "Freescale","Freescale:11",
                      "Freescale Semiconductor", "Freescale Semiconductor:11",
                      "Freescale Semiconductors", "Freescale Semiconductors:11",
                      "NXP (founded by Philips)", "NXP (founded by Philips):11",
                      "nxp:78" }, true);

  CheckVendorMatch({ "Silicon Labs", "Silicon Labs:21", "Energy Micro", "Energy Micro:21",
                       "Silicon Laboratories, Inc.", "Silicon Laboratories, Inc.:21", "Test:97" }, true);

  CheckVendorMatch({ "MyVendor", "MyVendor", "MyVendor:9999" }, true);

  CheckVendorMatch({ "MyVendor", "ThatVendor"}, false);
  CheckVendorMatch({ "MyVendor:9999", "ThatVendor:9998" }, false);
  CheckVendorMatch({ "MyVendor:9999", "MyVendor:9998" }, true);
  CheckVendorMatch({ "MyVendor:9999", "ThatVendor:9999" }, true);
}

TEST(RteUtilsTest, GetFullVendorString)
{
  EXPECT_EQ("unknown", DeviceVendor::GetFullVendorString("unknown"));
  EXPECT_EQ("unknown:0000", DeviceVendor::GetFullVendorString("unknown:0000"));

  EXPECT_EQ("NXP:11", DeviceVendor::GetFullVendorString("NXP:11"));
  EXPECT_EQ("NXP:11", DeviceVendor::GetFullVendorString("NXP"));
  EXPECT_EQ("NXP:11", DeviceVendor::GetFullVendorString("anything:11"));

  EXPECT_EQ("NXP:11", DeviceVendor::GetFullVendorString("Freescale:78"));
  EXPECT_EQ("NXP:11", DeviceVendor::GetFullVendorString("Freescale"));

  EXPECT_EQ("NXP:11", DeviceVendor::GetFullVendorString(":78"));

  EXPECT_EQ("Cypress:19", DeviceVendor::GetFullVendorString("Fujitsu::114"));
  EXPECT_EQ("Cypress:19", DeviceVendor::GetFullVendorString("Spansion::100"));

  EXPECT_EQ("Silicon Labs:21", DeviceVendor::GetFullVendorString("EnergyMicro::97"));
}

TEST(RteUtilsTest, VendorFromPackageId)
{
  string packageId, vendor;
  packageId = "Vendor.Name.version";
  vendor =  RteUtils::VendorFromPackageId(packageId);
  EXPECT_EQ(vendor, "Vendor");

  packageId = "VendorNameversion";
  vendor = RteUtils::VendorFromPackageId(packageId);
  EXPECT_EQ(false, (0 == vendor.compare("vendor")) ? true : false);
}

TEST(RteUtilsTest, NameFromPackageId)
{
  string packageId, name;
  packageId = "Vendor.Name.Version";
  name = RteUtils::NameFromPackageId(packageId);
  EXPECT_EQ(name, "Name");

  packageId = "VendorNameversion";
  name = RteUtils::NameFromPackageId(packageId);
  EXPECT_EQ(false, (0 == name.compare("Name")) ? true : false);
}

TEST(RteUtilsTest, RemoveVectorDuplicates)
{
  vector<int> testInput = { 1,2,3,2,4,5,3,6 };
  vector<int> expected  = { 1,2,3,4,5,6 };
  RteUtils::RemoveVectorDuplicates<int>(testInput);
  EXPECT_EQ(testInput, expected);

  testInput = { 1,1,3,3,1,2,2 };
  expected = { 1,3,2 };
  RteUtils::RemoveVectorDuplicates<int>(testInput);
  EXPECT_EQ(testInput, expected);

  testInput = { 1,1,1,1,1,1,1,1,1 };
  expected = { 1 };
  RteUtils::RemoveVectorDuplicates<int>(testInput);
  EXPECT_EQ(testInput, expected);

  testInput = { };
  expected = { };
  RteUtils::RemoveVectorDuplicates<int>(testInput);
  EXPECT_EQ(testInput, expected);
}
