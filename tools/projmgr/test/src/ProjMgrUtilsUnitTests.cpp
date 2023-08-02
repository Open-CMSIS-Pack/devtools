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

TEST_F(ProjMgrUtilsUnitTests, ExecCommand) {
  auto result = ProjMgrUtils::ExecCommand("invalid command");
  EXPECT_EQ(false, (0 == result.second) ? true : false) << result.first;

  string testdir = "mkdir_test_dir";
  error_code ec;
  const auto& workingDir = fs::current_path(ec);
  fs::current_path(testoutput_folder, ec);
  if (fs::exists(testdir)) {
    RteFsUtils::RemoveDir(testdir);
  }
  result = ProjMgrUtils::ExecCommand("mkdir " + testdir);
  EXPECT_TRUE(fs::exists(testdir));
  EXPECT_EQ(true, (0 == result.second) ? true : false) << result.first;

  fs::current_path(workingDir, ec);
  RteFsUtils::RemoveDir(testdir);
}

TEST_F(ProjMgrUtilsUnitTests, StrToInt) {
  map<string, int> testDataVec = {
    { "0", 0 },
    { " ", 0 },
    { "", 0 },
    { "alphanum012345", 0 },
    { "000", 0 },
    { "123", 123 },
    { "+456", 456 },
  };
  for (const auto& [input, expected] : testDataVec) {
    EXPECT_EQ(ProjMgrUtils::StringToInt(input), expected);
  }
}

TEST_F(ProjMgrUtilsUnitTests, GetCategory) {
  map<string, vector<string>> testDataVec = {
    {"sourceC",      {"sourceFile.c", "sourceFile.C"}},
    {"sourceCpp",    {"sourceFile.cpp", "sourceFile.c++", "sourceFile.C++", "sourceFile.cxx", "sourceFile.cc", "sourceFile.CC"}},
    {"sourceAsm",    {"sourceFile.asm", "sourceFile.s", "sourceFile.S"}},
    {"header",       {"headerFile.h", "headerFile.hpp"}},
    {"library",      {"libraryFile.a", "libraryFile.lib"}},
    {"object",       {"objectFile.o"}},
    {"linkerScript", {"linkerFile.sct", "linkerFile.scf", "linkerFile.ld", "linkerFile.icf"}},
    {"doc",          {"documentFile.txt", "documentFile.md", "documentFile.pdf", "documentFile.htm", "documentFile.html"}},
  };
  for (const auto& [expected, files] : testDataVec) {
    for (const auto& file : files) {
      EXPECT_EQ(ProjMgrUtils::GetCategory(file), expected);
    }
  }
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
    { {"Unknown"},                       Error("Following context name(s) was not found:\n  Unknown"), {}},
    { {".UnknownBuild"},                 Error("Following context name(s) was not found:\n  .UnknownBuild"), {}},
    { {"+UnknownTarget"},                Error("Following context name(s) was not found:\n  +UnknownTarget"), {}},
    { {"Project.UnknownBuild"},          Error("Following context name(s) was not found:\n  Project.UnknownBuild"), {}},
    { {"Project+UnknownTarget"},         Error("Following context name(s) was not found:\n  Project+UnknownTarget"), {}},
    { {".UnknownBuild+Target"},          Error("Following context name(s) was not found:\n  .UnknownBuild+Target"), {}},
    { {"TestProject*"},                  Error("Following context name(s) was not found:\n  TestProject*"), {}},
    { {"Project.*Build"},                Error("Following context name(s) was not found:\n  Project.*Build"), {}},
    { {"Project.Debug+*H"},              Error("Following context name(s) was not found:\n  Project.Debug+*H"), {}},
    { {"Project1.Release.Debug+Target"}, Error("Following context name(s) was not found:\n  Project1.Release.Debug+Target"), {}},
    { {"Project1.Debug+Target+Target2"}, Error("Following context name(s) was not found:\n  Project1.Debug+Target+Target2"), {}},
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

TEST_F(ProjMgrUtilsUnitTests, ReplaceContexts) {
  const vector<string> allContexts_1 = {
    "Project1.Debug+Target",
    "Project1.Release+Target",
    "Project1.Debug+Target2",
    "Project1.Release+Target2",
    "Project2.Debug+Target",
    "Project2.Release+Target",
    "Project2.Debug+Target2",
    "Project2.Release+Target2",
  };

  const vector<string> allContexts_2 = {
    "Project1.Debug+Target1",
    "Project1.Release+Target1",
    "Project2.Debug+Target1",
    "Project2.Debug+Target2",
  };

  const vector<string> allContexts_3 = {
    "test1.Debug+CM3",
    "test1.Release+CM3",
    "test1.Test+CM3",
    "test2.Debug+CM3",
    "test2.Release+CM3",
    "test2.Test+CM3",
  };

  const vector<string> allContexts_4 = {
    "Project1.Debug1+Target",
    "Project1.Debug2+Target2",
    "Project1.Release1+Target",
    "Project1.Release2+Target2",
    "Project2.Debug1+Target",
    "Project2.Debug2+Target2",
    "Project2.Release2+Target",
    "Project2.Release2+Target2",
  };

  struct TestCase {
    vector<string>  inputAllContexts;
    vector<string>  contextFilters;
    string          contextReplacement;
    vector<string>  expectedSelectedContexts;
    Error           expectedOutError;

    string toString() const {
      string input;
      std::for_each(contextFilters.begin(), contextFilters.end(),
        [&](const std::string& item) { input += "--context " + item + " "; });
      return input + "--context-replacement " + contextReplacement;
    }
  };

  vector<TestCase> vecTestCase = {
    // Positive tests
    { allContexts_1, {""},                      "", allContexts_1, {}},
    { allContexts_1, {""},                      "+*", allContexts_1, {}},
    { allContexts_1, {"Proj*"},                 "Proj*", allContexts_1, {}},
    { allContexts_1, {".Release"},              ".Release", {"Project1.Release+Target", "Project1.Release+Target2", "Project2.Release+Target", "Project2.Release+Target2"}, {}},
    { allContexts_1, {"Project2.Debug+Target"}, "Project2.Release+Target", {"Project2.Release+Target"}, {}},
    { allContexts_1, {"Project1.Debug"},        "Project1.Release", {"Project1.Release+Target","Project1.Release+Target2"}, {}},
    { allContexts_1, {"+Target2"},              "+Target", {"Project1.Debug+Target","Project1.Release+Target", "Project2.Debug+Target","Project2.Release+Target"}, {}},
    { allContexts_1, {"*.Debug", "*.Release"},  "*+Target*", {"Project1.Debug+Target", "Project1.Debug+Target2", "Project2.Debug+Target", "Project2.Debug+Target2", "Project1.Release+Target", "Project1.Release+Target2", "Project2.Release+Target", "Project2.Release+Target2" }, {}},
    { allContexts_1, {"Project2.Debug+Target"}, "Project2.Release+Target2", {"Project2.Release+Target2"}, {}},
    { allContexts_4, {"Project*.Rel*"},         "Project*.Debug*", {"Project1.Debug1+Target", "Project1.Debug2+Target2", "Project2.Debug1+Target", "Project2.Debug2+Target2"}, {}},

    // Negative tests
    { allContexts_1, {"Project2"},              "Project1.Release", {}, Error("no suitable replacement found for context replacement \"Project1.Release\"")},
    { allContexts_1, {"Project1.Debug"},        "Project1.UnknownBuildType", {}, Error("invalid context replacement name. \"Project1.UnknownBuildType\" was not found.\n")},
    { allContexts_1, {".Debug", ".Release"},    ".Release", {}, Error("incompatible replacements found for \".Release\"")},
    { allContexts_1, {"Project1"},              ".Release", {}, Error("incompatible replacements found for \".Release\"")},
    { allContexts_1, {"Project2+Target2"},      "+Target2", {}, Error("invalid replacement request. Replacement contexts are more than the selected contexts")},
    { allContexts_2, {"Project2"},              ".Release", {}, Error("no suitable replacements found for context replacement \".Release\"")},
    { allContexts_2, {"*.Debug"},               ".Release", {}, Error("incompatible replacements found for \".Release\"")},
    { allContexts_3, {"+CM3"},                  ".Test", {}, Error("incompatible replacements found for \".Test\"")},
    { allContexts_3, {"test2"},                 "test2.Test", {}, Error("incompatible replacements found for \"test2.Test\"")},
    { allContexts_3, {"*.Debug"},               "+CM4", {}, Error("invalid context replacement name. \"+CM4\" was not found.")},
  };

  const auto ValidateOutput = [&](const auto& selectedContexts, const Error& outError,
    const TestCase& testCase) -> testing::AssertionResult
  {
    if (testCase.expectedOutError != outError) {
      return testing::AssertionFailure() << "expected error: " << testCase.expectedOutError.m_errMsg;
    }
    if (string::npos == outError.m_errMsg.find(testCase.expectedOutError.m_errMsg)) {
      return testing::AssertionFailure() << "expected error: " << testCase.expectedOutError.m_errMsg;
    }
    if (testCase.expectedSelectedContexts != selectedContexts) {
      return testing::AssertionFailure() << "selected and expected contexts don't match";
    }
    return testing::AssertionSuccess();
  };

  vector<string> selectedContexts;
  for (const auto& test : vecTestCase) {
    selectedContexts.clear();
    ASSERT_EQ(false, GetSelectedContexts(selectedContexts, test.inputAllContexts, test.contextFilters));
    auto outError = ReplaceContexts(selectedContexts, test.inputAllContexts, test.contextReplacement);

    EXPECT_TRUE(ValidateOutput(selectedContexts, outError, test)) <<
      "failed for input \"" << test.toString() << "\"";
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
  };

  vector<std::pair<string, const vector<string>>> vecTestData = {
    // contextFilter, expectedContexts
    { "",                         allContexts},
    { "Project1",                 { "Project1.Debug+Target","Project1.Release+Target","Project1.Debug+Target2","Project1.Release+Target2"}},
    { ".Debug",                   { "Project1.Debug+Target","Project1.Debug+Target2","Project2.Debug+Target","Project2.Debug+Target2"}},
    { "+Target",                  { "Project1.Debug+Target", "Project1.Release+Target", "Project2.Debug+Target", "Project2.Release+Target"}},
    { "Project1.Debug",           { "Project1.Debug+Target", "Project1.Debug+Target2" }},
    { "Project1+Target",          { "Project1.Debug+Target", "Project1.Release+Target" }},
    { ".Release+Target2",         { "Project1.Release+Target2", "Project2.Release+Target2" }},
    { "Project1.Release+Target2", { "Project1.Release+Target2" }},

    { "*",                        allContexts},
    { "*.*+*",                    allContexts},
    { "*.*",                      allContexts},
    { "Proj*",                    allContexts},
    { ".De*",                     { "Project1.Debug+Target","Project1.Debug+Target2","Project2.Debug+Target","Project2.Debug+Target2"}},
    { "+Tar*",                    allContexts},
    { "Proj*.D*g",                { "Project1.Debug+Target","Project1.Debug+Target2","Project2.Debug+Target","Project2.Debug+Target2"}},
    { "Proj*+Tar*",               allContexts},
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
