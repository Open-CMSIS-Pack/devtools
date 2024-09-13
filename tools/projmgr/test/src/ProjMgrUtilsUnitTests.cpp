/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "ProjMgrUtils.h"
#include "RteFsUtils.h"
#include "RtePackage.h"
#include "RteGenerator.h"

#include "CrossPlatformUtils.h"

#include "gtest/gtest.h"

using namespace std;

class ProjMgrUtilsUnitTests : public ProjMgrUtils, public ::testing::Test {
protected:
  ProjMgrUtilsUnitTests() {}
  virtual ~ProjMgrUtilsUnitTests() {}
};

TEST_F(ProjMgrUtilsUnitTests, GetPackageID) {
  const map<string, string> attributes = {
    {"vendor" , "Vendor"  },
    {"name"   , "Name"   },
    {"version", "8.8.8"   }
  };
  RteItem item(attributes);
  item.SetTag("require");
  EXPECT_EQ("Vendor::Name@8.8.8", item.GetPackageID());
}

TEST_F(ProjMgrUtilsUnitTests, ReadGpdscFile) {
  const string& gpdscFile = testinput_folder + "/TestGenerator/RTE/Device/RteTestGen_ARMCM0/RteTest.gpdsc";
  bool validGpdsc;
  RtePackage* gpdscPack = ProjMgrUtils::ReadGpdscFile(gpdscFile, validGpdsc);
  ASSERT_NE(gpdscPack, nullptr);
  EXPECT_TRUE(validGpdsc);
  RteGenerator* gen = gpdscPack->GetFirstGenerator();
  ASSERT_NE(gen, nullptr);
  EXPECT_EQ("RteTestGeneratorIdentifier", gen->GetName());
  delete gpdscPack;
}

TEST_F(ProjMgrUtilsUnitTests, ReadGpdscFile_Warning) {
  const string& gpdscFile = testinput_folder + "/TestGenerator/RTE/Device/RteTestGen_ARMCM0/RteTest_Warning.gpdsc";
  bool validGpdsc;
  RtePackage* gpdscPack = ProjMgrUtils::ReadGpdscFile(gpdscFile, validGpdsc);
  ASSERT_NE(gpdscPack, nullptr);
  EXPECT_FALSE(validGpdsc);
  RteGenerator* gen = gpdscPack->GetFirstGenerator();
  ASSERT_NE(gen, nullptr);
  EXPECT_EQ("RteTestGeneratorIdentifier", gen->GetName());
  delete gpdscPack;
}

TEST_F(ProjMgrUtilsUnitTests, ReadGpdscFile_Invalid) {
  const string& gpdscFile = testinput_folder + "/TestGenerator/RTE/Device/RteTestGen_ARMCM0/RteTest_Invalid.gpdsc";
  bool validGpdsc;
  RtePackage* gpdscPack = ProjMgrUtils::ReadGpdscFile(gpdscFile, validGpdsc);
  ASSERT_EQ(gpdscPack, nullptr);
  EXPECT_FALSE(validGpdsc);
}

TEST_F(ProjMgrUtilsUnitTests, ReadGpdscFileNoExists) {
  const string& gpdscFile = testinput_folder + "/TestGenerator/NonExisting.gpdsc";
  bool validGpdsc;
  RtePackage* gpdscPack = ProjMgrUtils::ReadGpdscFile(gpdscFile, validGpdsc);
  ASSERT_EQ(gpdscPack, nullptr);
  EXPECT_FALSE(validGpdsc);
}

TEST_F(ProjMgrUtilsUnitTests, CompilersIntersect) {
  string intersection;
  CompilersIntersect("AC6@6.16.0", "AC6", intersection);
  EXPECT_EQ("AC6@6.16.0", intersection);
  intersection.clear();
  CompilersIntersect("AC6@>=6.16.0", "AC6", intersection);
  EXPECT_EQ("AC6@>=6.16.0", intersection);
  intersection.clear();
  CompilersIntersect("AC6@>=6.6.5", "AC6@6.16.0", intersection);
  EXPECT_EQ("AC6@6.16.0", intersection);
  intersection.clear();
  CompilersIntersect("AC6@>=6.6.5", "AC6@6.6.5", intersection);
  EXPECT_EQ("AC6@6.6.5", intersection);
  intersection.clear();
  CompilersIntersect("AC6@>=6.6.5", "AC6@>=6.16.0", intersection);
  EXPECT_EQ("AC6@>=6.16.0", intersection);
  intersection.clear();
  CompilersIntersect("AC6@>=6.6.5", "", intersection);
  EXPECT_EQ("AC6@>=6.6.5", intersection);
  intersection.clear();
  CompilersIntersect("GCC@0.0.0", "", intersection);
  EXPECT_EQ("GCC@0.0.0", intersection);
  intersection.clear();
  CompilersIntersect("", "", intersection);
  EXPECT_EQ("", intersection);
  intersection.clear();
  CompilersIntersect("AC6@6.6.5", "AC6@6.16.0", intersection);
  EXPECT_EQ("", intersection);
  intersection.clear();
  CompilersIntersect("AC6@6.6.5", "AC6@>=6.16.0", intersection);
  EXPECT_EQ("", intersection);
  intersection.clear();
  CompilersIntersect("GCC@6.16.0", "AC6@6.16.0", intersection);
  EXPECT_EQ("", intersection);
  intersection.clear();
  CompilersIntersect("GCC", "AC6", intersection);
  EXPECT_EQ("", intersection);
  intersection.clear();
};

TEST_F(ProjMgrUtilsUnitTests, AreCompilersCompatible) {
  EXPECT_TRUE(AreCompilersCompatible("AC6@6.16.0", "AC6"));
  EXPECT_TRUE(AreCompilersCompatible("AC6@>=6.16.0", "AC6"));
  EXPECT_TRUE(AreCompilersCompatible("AC6@>=6.6.5", "AC6@6.16.0"));
  EXPECT_TRUE(AreCompilersCompatible("AC6@>=6.6.5", "AC6@6.6.5"));
  EXPECT_TRUE(AreCompilersCompatible("AC6@>=6.6.5", "AC6@>=6.16.0"));
  EXPECT_TRUE(AreCompilersCompatible("AC6@>=6.6.5", ""));
  EXPECT_TRUE(AreCompilersCompatible("", ""));
  EXPECT_FALSE(AreCompilersCompatible("AC6@6.6.5", "AC6@6.16.0"));
  EXPECT_FALSE(AreCompilersCompatible("AC6@6.6.5", "AC6@>=6.16.0"));
  EXPECT_FALSE(AreCompilersCompatible("GCC@6.16.0", "AC6@6.16.0"));
  EXPECT_FALSE(AreCompilersCompatible("GCC", "AC6"));
};

TEST_F(ProjMgrUtilsUnitTests, ParseContextEntry) {
  auto compare = [](const ContextName& context, const ContextName& expected) {
    return ((context.project == expected.project) && (context.build == expected.build) && (context.target == expected.target));
  };

  auto TestParseContextEntry = [&](const string& context, const ContextName& expected) -> bool {
    ContextName outContext;
    auto retVal = ParseContextEntry(context, outContext);
    return retVal && compare(outContext, expected);
  };

  EXPECT_TRUE (TestParseContextEntry("project"               , { "project", "",      ""       }));
  EXPECT_TRUE (TestParseContextEntry("project.build"         , { "project", "build", ""       }));
  EXPECT_TRUE (TestParseContextEntry("project+target"        , { "project", ""     , "target" }));
  EXPECT_TRUE (TestParseContextEntry("project.build+target"  , { "project", "build", "target" }));
  EXPECT_TRUE (TestParseContextEntry("project+target.build"  , { "project", "build", "target" }));
  EXPECT_TRUE (TestParseContextEntry(".build"                , { ""       , "build", ""       }));
  EXPECT_TRUE (TestParseContextEntry(".build+target"         , { ""       , "build", "target" }));
  EXPECT_TRUE (TestParseContextEntry("+target"               , { ""       , ""     , "target" }));
  EXPECT_TRUE (TestParseContextEntry("+target.build"         , { ""       , "build", "target" }));
  EXPECT_TRUE (TestParseContextEntry(""                      , { ""       , ""     , ""       }));
  EXPECT_TRUE (TestParseContextEntry(".bu*d+tar*"            , { ""       , "bu*d" , "tar*"   }));
  EXPECT_FALSE(TestParseContextEntry(".build1.build2+target" , { ""       , ""     , ""       }));
  EXPECT_FALSE(TestParseContextEntry(".build1+target+target1", { ""       , ""     , ""       }));
};

TEST_F(ProjMgrUtilsUnitTests, GetSelectedContexts) {
  const vector<string> allContexts = {
    "Project1.Debug+Target",
    "Project1.Release+Target",
    "Project1.Debug+Target2",
    "Project1.Release+Target2",
    "Project2.Debug+Target",
    "Project2.Release+Target",
    "Project2.Debug+Target2",
    "Project2.Release+Target2",
  };

  Error noError;
  vector<std::tuple<vector<string>, Error, const vector<string>>> vecTestData = {
    // contextFilter, expectedRetval, expectedContexts
    { {""},                         noError, allContexts},
    { {"Project1"},                 noError, { "Project1.Debug+Target","Project1.Release+Target","Project1.Debug+Target2","Project1.Release+Target2"}},
    { {".Debug"},                   noError, { "Project1.Debug+Target","Project1.Debug+Target2","Project2.Debug+Target","Project2.Debug+Target2"}},
    { {"+Target"},                  noError, { "Project1.Debug+Target", "Project1.Release+Target", "Project2.Debug+Target", "Project2.Release+Target"}},
    { {"Project1.Debug"},           noError, { "Project1.Debug+Target", "Project1.Debug+Target2" }},
    { {"Project1+Target"},          noError, { "Project1.Debug+Target", "Project1.Release+Target" }},
    { {".Release+Target2"},         noError, { "Project1.Release+Target2", "Project2.Release+Target2" }},
    { {"Project1.Release+Target2"}, noError, { "Project1.Release+Target2" }},

    { {"*"},                        noError, allContexts},
    { {"*.*+*"},                    noError, allContexts},
    { {"*.*"},                      noError, allContexts},
    { {"Proj*"},                    noError, allContexts},
    { {".De*"},                     noError, { "Project1.Debug+Target","Project1.Debug+Target2","Project2.Debug+Target","Project2.Debug+Target2"}},
    { {"+Tar*"},                    noError, allContexts},
    { {"Proj*.D*g"},                noError, { "Project1.Debug+Target","Project1.Debug+Target2","Project2.Debug+Target","Project2.Debug+Target2"}},
    { {"Proj*+Tar*"},               noError, allContexts},
    { {"Project2.Rel*+Tar*"},       noError, {"Project2.Release+Target", "Project2.Release+Target2"}},
    { {".Rel*+*2"},                 noError, {"Project1.Release+Target2", "Project2.Release+Target2"}},
    { {"Project*.Release+*"},       noError, {"Project1.Release+Target", "Project1.Release+Target2","Project2.Release+Target", "Project2.Release+Target2"}},

    // negative tests
    { {"Unknown"},                       Error("no matching context found for option:\n  --context Unknown"), {}},
    { {".UnknownBuild"},                 Error("no matching context found for option:\n  --context .UnknownBuild"), {}},
    { {"+UnknownTarget"},                Error("no matching context found for option:\n  --context +UnknownTarget"), {}},
    { {"Project.UnknownBuild"},          Error("no matching context found for option:\n  --context Project.UnknownBuild"), {}},
    { {"Project+UnknownTarget"},         Error("no matching context found for option:\n  --context Project+UnknownTarget"), {}},
    { {".UnknownBuild+Target"},          Error("no matching context found for option:\n  --context .UnknownBuild+Target"), {}},
    { {"TestProject*"},                  Error("no matching context found for option:\n  --context TestProject*"), {}},
    { {"Project.*Build"},                Error("no matching context found for option:\n  --context Project.*Build"), {}},
    { {"Project.Debug+*H"},              Error("no matching context found for option:\n  --context Project.Debug+*H"), {}},
    { {"Project1.Release.Debug+Target"}, Error("no matching context found for option:\n  --context Project1.Release.Debug+Target"), {}},
    { {"Project1.Debug+Target+Target2"}, Error("no matching context found for option:\n  --context Project1.Debug+Target+Target2"), {}},
  };

  vector<string> selectedContexts;
  for (const auto& [contextFilters, expectedError, expectedContexts] : vecTestData) {
    string input;
    selectedContexts.clear();
    std::for_each(contextFilters.begin(), contextFilters.end(),
      [&](const std::string& item) { input += item + " "; });
    const auto& outError = GetSelectedContexts(selectedContexts, allContexts, contextFilters);
    EXPECT_EQ(expectedError, outError) << "failed for input \"" << input << "\"";
    EXPECT_NE(string::npos, outError.m_errMsg.find(expectedError.m_errMsg)) << "failed for input \"" << input << "\"";
    EXPECT_EQ(selectedContexts, expectedContexts);
  }
}

TEST_F(ProjMgrUtilsUnitTests, GetFilteredContexts) {
  const vector<string> allContexts = {
    "Project1.Debug+Target",
    "Project1.Release+Target",
    "Project1.Debug+Target2",
    "Project1.Release+Target2",
    "Project2.Debug+Target",
    "Project2.Release+Target",
    "Project2.Debug+Target2",
    "Project2.Release+Target2",
    "Project3.Debug",
    "Project4+Target",
  };

  vector<std::pair<string, const vector<string>>> vecTestData = {
    // contextFilter, expectedContexts
    { "",                         allContexts},
    { "Project1",                 { "Project1.Debug+Target","Project1.Release+Target","Project1.Debug+Target2","Project1.Release+Target2"}},
    { ".Debug",                   { "Project1.Debug+Target","Project1.Debug+Target2","Project2.Debug+Target","Project2.Debug+Target2","Project3.Debug"}},
    { "+Target",                  { "Project1.Debug+Target", "Project1.Release+Target", "Project2.Debug+Target", "Project2.Release+Target","Project4+Target"}},
    { "Project1.Debug",           { "Project1.Debug+Target", "Project1.Debug+Target2" }},
    { "Project1+Target",          { "Project1.Debug+Target", "Project1.Release+Target" }},
    { ".Release+Target2",         { "Project1.Release+Target2", "Project2.Release+Target2" }},
    { "Project1.Release+Target2", { "Project1.Release+Target2" }},

    { "*",                        allContexts},
    { "*.*+*",                    allContexts},
    { "*.*",                      allContexts},
    { "Proj*",                    allContexts},
    { ".De*",                     { "Project1.Debug+Target","Project1.Debug+Target2","Project2.Debug+Target","Project2.Debug+Target2","Project3.Debug"}},
    { "+Tar*",                    { "Project1.Debug+Target","Project1.Release+Target","Project1.Debug+Target2","Project1.Release+Target2","Project2.Debug+Target",
                                    "Project2.Release+Target","Project2.Debug+Target2","Project2.Release+Target2","Project4+Target"} },
    { "Proj*.D*g",                { "Project1.Debug+Target","Project1.Debug+Target2","Project2.Debug+Target","Project2.Debug+Target2","Project3.Debug"}},
    { "Proj*+Tar*",               { "Project1.Debug+Target","Project1.Release+Target","Project1.Debug+Target2","Project1.Release+Target2","Project2.Debug+Target",
                                    "Project2.Release+Target","Project2.Debug+Target2","Project2.Release+Target2","Project4+Target" }},
    { "Project2.Rel*+Tar*",       {"Project2.Release+Target", "Project2.Release+Target2"}},
    { ".Rel*+*2",                 {"Project1.Release+Target2", "Project2.Release+Target2"}},
    { "Project*.Release+*",       {"Project1.Release+Target", "Project1.Release+Target2","Project2.Release+Target", "Project2.Release+Target2"}},

    // negative tests
    { "Unknown",                       RteUtils::EMPTY_STRING_VECTOR},
    { ".UnknownBuild",                 RteUtils::EMPTY_STRING_VECTOR},
    { "+UnknownTarget",                RteUtils::EMPTY_STRING_VECTOR},
    { "Project.UnknownBuild",          RteUtils::EMPTY_STRING_VECTOR},
    { "Project+UnknownTarget",         RteUtils::EMPTY_STRING_VECTOR},
    { ".UnknownBuild+Target",          RteUtils::EMPTY_STRING_VECTOR},
    { "+Debug",                        RteUtils::EMPTY_STRING_VECTOR},
    { ".Target",                       RteUtils::EMPTY_STRING_VECTOR},
    { "TestProject*",                  RteUtils::EMPTY_STRING_VECTOR},
    { "Project.*Build",                RteUtils::EMPTY_STRING_VECTOR},
    { "Project.Debug+*H",              RteUtils::EMPTY_STRING_VECTOR},
    { "Project1.Release.Debug+Target", RteUtils::EMPTY_STRING_VECTOR},
    { "Project1.Debug+Target+Target2", RteUtils::EMPTY_STRING_VECTOR},
  };

  for (const auto& [contextFilter, expectedContexts] : vecTestData) {
    EXPECT_EQ(expectedContexts, GetFilteredContexts(allContexts, contextFilter)) <<
      "failed for input \"" << contextFilter << "\"";
  }
}

TEST_F(ProjMgrUtilsUnitTests, ConvertToPackInfo) {
  PackInfo packInfo;

  packInfo = {"", "", ""};
  EXPECT_TRUE(ProjMgrUtils::ConvertToPackInfo("", packInfo));
  EXPECT_EQ("", packInfo.vendor);
  EXPECT_EQ("", packInfo.name);
  EXPECT_EQ("", packInfo.version);

  packInfo = {"", "", ""};
  EXPECT_TRUE(ProjMgrUtils::ConvertToPackInfo("ARM", packInfo));
  EXPECT_EQ("ARM", packInfo.vendor);
  EXPECT_EQ("", packInfo.name);
  EXPECT_EQ("", packInfo.version);

  packInfo = {"", "", ""};
  EXPECT_TRUE(ProjMgrUtils::ConvertToPackInfo("ARM@5.8.0", packInfo));
  EXPECT_EQ("ARM", packInfo.vendor);
  EXPECT_EQ("", packInfo.name);
  EXPECT_EQ("5.8.0", packInfo.version);

  packInfo = {"", "", ""};
  EXPECT_TRUE(ProjMgrUtils::ConvertToPackInfo("ARM@>=5.8.0", packInfo));
  EXPECT_EQ("ARM", packInfo.vendor);
  EXPECT_EQ("", packInfo.name);
  EXPECT_EQ(">=5.8.0", packInfo.version);

  packInfo = {"", "", ""};
  EXPECT_TRUE(ProjMgrUtils::ConvertToPackInfo("ARM::CMSIS", packInfo));
  EXPECT_EQ("ARM", packInfo.vendor);
  EXPECT_EQ("CMSIS", packInfo.name);
  EXPECT_EQ("", packInfo.version);

  packInfo = {"", "", ""};
  EXPECT_TRUE(ProjMgrUtils::ConvertToPackInfo("ARM::CMSIS@5.8.0", packInfo));
  EXPECT_EQ("ARM", packInfo.vendor);
  EXPECT_EQ("CMSIS", packInfo.name);
  EXPECT_EQ("5.8.0", packInfo.version);

  packInfo = {"", "", ""};
  EXPECT_TRUE(ProjMgrUtils::ConvertToPackInfo("ARM::CMSIS@>=5.8.0", packInfo));
  EXPECT_EQ("ARM", packInfo.vendor);
  EXPECT_EQ("CMSIS", packInfo.name);
  EXPECT_EQ(">=5.8.0", packInfo.version);
}

TEST_F(ProjMgrUtilsUnitTests, IsMatchingPackInfo) {

  // Vendor
  EXPECT_TRUE (ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"", "ARM", ""}));

  // Wrong name
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test", "ARM", ""}));

  // Vendor + exact version
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"", "ARM", "5.7.0"}));
  EXPECT_TRUE (ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"", "ARM", "5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"", "ARM", "5.9.0"}));

  // Vendor + ranges
  EXPECT_TRUE (ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"", "ARM", ">=5.7.0"}));
  EXPECT_TRUE (ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"", "ARM", ">=5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"", "ARM", ">=5.9.0"}));

  // Vendor + wildcard name
  EXPECT_TRUE (ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CM*", "ARM", ""}));

  // Vendor + wildcard name + exact version
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CM*", "ARM", "5.7.0"}));
  EXPECT_TRUE (ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CM*", "ARM", "5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CM*", "ARM", "5.9.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSI.", "ARM", "5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "AR.", "5.8.0"}));

  // Vendor + wildcard name + ranges
  EXPECT_TRUE (ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CM*", "ARM", ">=5.7.0"}));
  EXPECT_TRUE (ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CM*", "ARM", ">=5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CM*", "ARM", ">=5.9.0"}));

  // Vendor + wrong wildcard name
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test*", "ARM", ""}));

  // Vendor + wrong wildcard name + exact version
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test*", "ARM", "5.7.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test*", "ARM", "5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test*", "ARM", "5.9.0"}));

  // Vendor + wrong wildcard name + ranges
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test*", "ARM", ">=5.7.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test*", "ARM", ">=5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test*", "ARM", ">=5.9.0"}));

  // Vendor + name
  EXPECT_TRUE (ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "ARM", ""}));

  // Vendor + name + exact version
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "ARM", "5.7.0"}));
  EXPECT_TRUE (ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "ARM", "5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "ARM", "5.9.0"}));

  // Vendor + name + ranges
  EXPECT_TRUE (ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "ARM", ">=5.7.0"}));
  EXPECT_TRUE (ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "ARM", ">=5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "ARM", ">=5.9.0"}));



  // Wrong vendor
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"", "Test", ""}));

  // Wrong vendor + wrong name
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test", "Test", ""}));

  // Wrong vendor + exact version
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"", "Test", "5.7.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"", "Test", "5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"", "Test", "5.9.0"}));

  // Wrong vendor + ranges
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"", "Test", ">=5.7.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"", "Test", ">=5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"", "Test", ">=5.9.0"}));

  // Wrong vendor + wildcard name
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CM*", "Test", ""}));

  // Wrong vendor + wildcard name + exact version
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CM*", "Test", "5.7.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CM*", "Test", "5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CM*", "Test", "5.9.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSI.", "Test", "5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "Tes.", "5.8.0"}));

  // Wrong vendor + wildcard name + ranges
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CM*", "Test", ">=5.7.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CM*", "Test", ">=5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CM*", "Test", ">=5.9.0"}));

  // Wrong vendor + wrong wildcard name
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test*", "Test", ""}));

  // Wrong vendor + wrong wildcard name + exact version
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test*", "Test", "5.7.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test*", "Test", "5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test*", "Test", "5.9.0"}));

  // Wrong vendor + wrong wildcard name + ranges
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test*", "Test", ">=5.7.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test*", "Test", ">=5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"Test*", "Test", ">=5.9.0"}));

  // Wrong vendor + name
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "Test", ""}));

  // Wrong vendor + name + exact version
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "Test", "5.7.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "Test", "5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "Test", "5.9.0"}));

  // Wrong vendor + name + ranges
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "Test", ">=5.7.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "Test", ">=5.8.0"}));
  EXPECT_FALSE(ProjMgrUtils::IsMatchingPackInfo({"CMSIS", "ARM", "5.8.0"}, {"CMSIS", "Test", ">=5.9.0"}));
}

TEST_F(ProjMgrUtilsUnitTests, ConvertToVersionRange) {
  EXPECT_EQ("", ProjMgrUtils::ConvertToVersionRange(""));
  EXPECT_EQ("1.2.3:1.2.3", ProjMgrUtils::ConvertToVersionRange("1.2.3"));
  EXPECT_EQ("1.2.3", ProjMgrUtils::ConvertToVersionRange(">=1.2.3"));
  EXPECT_EQ("1.2.3-build4", ProjMgrUtils::ConvertToVersionRange(">=1.2.3-build4"));
  EXPECT_EQ("1.2.3:1.3.0-0", ProjMgrUtils::ConvertToVersionRange("~1.2.3"));
  EXPECT_EQ("1.2.3:2.0.0-0", ProjMgrUtils::ConvertToVersionRange("^1.2.3"));
}

TEST_F(ProjMgrUtilsUnitTests, ReplaceDelimiters) {
  EXPECT_EQ("Cvendor_Cbundle_Cclass_Cgroup_Cvariant_Cversion", ProjMgrUtils::ReplaceDelimiters("Cvendor&Cbundle::Cclass:Cgroup&Cvariant@Cversion"));
  EXPECT_EQ("ARM_CMSIS_CORE_A", ProjMgrUtils::ReplaceDelimiters("ARM::CMSIS.CORE A"));
  EXPECT_EQ("AC6_6_16_0", ProjMgrUtils::ReplaceDelimiters("AC6@>=6.16.0"));
  EXPECT_EQ("path_with_spaces", ProjMgrUtils::ReplaceDelimiters("path/with spaces"));
}

TEST_F(ProjMgrUtilsUnitTests, FindReferencedContext) {
  const vector<string> selectedContexts = {
    "Project1.Debug+Target",
    "Project1.Debug+OtherTarget",
    "Project2.Release+Target",
    "Project2.Release+OtherTarget",
  };
  const string currentContext = "Project1.Debug+Target";
  const vector<pair<string, string>> testData = {
    {"Project1.Debug+Target"  , ""                             },
    {"Project1.Debug+Target"  , "Project1"                     },
    {"Project1.Debug+Target"  , "Project1+Target"              },
    {"Project1.Debug+Target"  , "Project1.Debug+Target"        },
    {"Project1.Debug+Target"  , ".Debug"                       },
    {"Project2.Release+Target", "Project2"                     },
    {"Project2.Release+Target", "Project2+Target"              },
    {"Project2.Release+Target", "Project2.Release+Target"      },
    {""                       , "Project2+UnknowTarget"        },
    {""                       , "Project2.UnknowBuild+Target"  },
    {""                       , "Project1+UnknowTarget"        },
    {""                       , "Project2.Debug"               },
    {""                       , "Project1.Release"             },
    {""                       , ".Release"                     },
  };

  for (auto [expected, refContext] : testData) {
    EXPECT_EQ(expected, ProjMgrUtils::FindReferencedContext(currentContext, refContext, selectedContexts))
      << "failed for refContext \"" << refContext << "\"";
  }
}

TEST_F(ProjMgrUtilsUnitTests, FormatPath) {
  ProjMgrKernel::Get()->SetCmsisPackRoot(testcmsispack_folder);
  vector<pair<string, string>> testData = {
    { "OriginalPath"                      , testoutput_folder + "/OriginalPath"     },
    { "${CMSIS_PACK_ROOT}/Pack"           , testcmsispack_folder + "/Pack"          },
    { "${CMSIS_COMPILER_ROOT}/Toolchain"  , testcmsiscompiler_folder + "/Toolchain" },
    { "https://www.url.com"               , "https://www.url.com"                   },
  };
  for (const auto& [expected, original] : testData) {
    EXPECT_EQ(expected, ProjMgrUtils::FormatPath(original, testoutput_folder))
      << "failed for original path \"" << original << "\"";
  }
  if (CrossPlatformUtils::GetHostType() == "win") {
    // in windows it must be case insensitive
    RteFsUtils::CreateDirectories(testoutput_folder + "/foobar");
    EXPECT_EQ("Folder", ProjMgrUtils::FormatPath(testoutput_folder + "/FooBar/Folder", testoutput_folder + "/FOOBAR"));
    // an absolute path relative to a different drive returns itself
    EXPECT_EQ("X:/Non_Existent/Absolute", ProjMgrUtils::FormatPath("X:/Non_Existent/Absolute", testoutput_folder));
  }
}
