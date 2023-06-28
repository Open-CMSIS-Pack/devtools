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

TEST_F(ProjMgrUtilsUnitTests, ComponentAttributesFromId) {

  string id = "Vendor::Class&Bundle:Group:Sub&Variant@9.9.9";
  RteItem item(ComponentAttributesFromId(id));
  EXPECT_EQ(id, GetComponentID(&item));

  id = "Class&Bundle:Group:Sub&Variant@9.9.9";
  item.SetAttributes(ComponentAttributesFromId(id));
  EXPECT_EQ(id, GetComponentID(&item));

  id = "Vendor::Class:Group&Variant";
  item.SetAttributes(ComponentAttributesFromId(id));
  EXPECT_EQ(id, GetComponentID(&item));

  id = "Class:Group:Sub&Variant";
  item.SetAttributes(ComponentAttributesFromId(id));
  EXPECT_EQ(id, GetComponentID(&item));

  id = "Class:Group:&Variant";
  item.SetAttributes(ComponentAttributesFromId(id));
  EXPECT_EQ("Class:Group&Variant", GetComponentID(&item));
}


TEST_F(ProjMgrUtilsUnitTests, GetComponentID) {
  const map<string, string> attributes = {
    {"Cvendor" , "Vendor"  },
    {"Cclass"  , "Class"   },
    {"Cbundle" , "Bundle"  },
    {"Cgroup"  , "Group"   },
    {"Csub"    , "Sub"     },
    {"Cvariant", "Variant" },
    {"Cversion", "9.9.9"   }
  };
  RteItem item(attributes);
  EXPECT_EQ("Vendor::Class&Bundle:Group:Sub&Variant@9.9.9",GetComponentID(&item));
}

TEST_F(ProjMgrUtilsUnitTests, GetComponentAggregateID) {
  const map<string, string> attributes = {
    {"Cvendor" , "Vendor"  },
    {"Cclass"  , "Class"   },
    {"Cbundle" , "Bundle"  },
    {"Cgroup"  , "Group"   },
    {"Csub"    , "Sub"     },
    {"Cvariant", "Variant" },
    {"Cversion", "9.9.9"   }
  };
  RteItem item(attributes);
  EXPECT_EQ("Vendor::Class&Bundle:Group:Sub", GetComponentAggregateID(&item));
}

TEST_F(ProjMgrUtilsUnitTests, GetConditionID) {
  const map<string, string> attributes = {
    {"Cvendor" , "Vendor"  },
    {"Cclass"  , "Class"   },
    {"Cgroup"  , "Group"   }
  };
  RteItem item(attributes);;
  item.SetTag("require");
  EXPECT_EQ("require Vendor::Class:Group", GetConditionID(&item));
}

TEST_F(ProjMgrUtilsUnitTests, GetPackageID) {
  const map<string, string> attributes = {
    {"vendor" , "Vendor"  },
    {"name"   , "Name"   },
    {"version", "8.8.8"   }
  };
  RteItem item(attributes);
  item.SetTag("require");
  EXPECT_EQ("Vendor::Name@8.8.8", GetPackageID(&item));
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

  list<string> allContextsList;
  allContextsList.assign(allContexts.begin(), allContexts.end());
  vector<string> emptyResult{};
  vector<std::tuple<vector<string>, vector<string>, const list<string>>> vecTestData = {
    // contextFilter, expectedRetval, expectedContexts
    { {""},                         emptyResult, allContextsList},
    { {"Project1"},                 emptyResult, { "Project1.Debug+Target","Project1.Release+Target","Project1.Debug+Target2","Project1.Release+Target2"}},
    { {".Debug"},                   emptyResult, { "Project1.Debug+Target","Project1.Debug+Target2","Project2.Debug+Target","Project2.Debug+Target2"}},
    { {"+Target"},                  emptyResult, { "Project1.Debug+Target", "Project1.Release+Target", "Project2.Debug+Target", "Project2.Release+Target"}},
    { {"Project1.Debug"},           emptyResult, { "Project1.Debug+Target", "Project1.Debug+Target2" }},
    { {"Project1+Target"},          emptyResult, { "Project1.Debug+Target", "Project1.Release+Target" }},
    { {".Release+Target2"},         emptyResult, { "Project1.Release+Target2", "Project2.Release+Target2" }},
    { {"Project1.Release+Target2"}, emptyResult, { "Project1.Release+Target2" }},

    { {"*"},                        emptyResult, allContextsList},
    { {"*.*+*"},                    emptyResult, allContextsList},
    { {"*.*"},                      emptyResult, allContextsList},
    { {"Proj*"},                    emptyResult, allContextsList},
    { {".De*"},                     emptyResult, { "Project1.Debug+Target","Project1.Debug+Target2","Project2.Debug+Target","Project2.Debug+Target2"}},
    { {"+Tar*"},                    emptyResult, allContextsList},
    { {"Proj*.D*g"},                emptyResult, { "Project1.Debug+Target","Project1.Debug+Target2","Project2.Debug+Target","Project2.Debug+Target2"}},
    { {"Proj*+Tar*"},               emptyResult, allContextsList},
    { {"Project2.Rel*+Tar*"},       emptyResult, {"Project2.Release+Target", "Project2.Release+Target2"}},
    { {".Rel*+*2"},                 emptyResult, {"Project1.Release+Target2", "Project2.Release+Target2"}},
    { {"Project*.Release+*"},       emptyResult, {"Project1.Release+Target", "Project1.Release+Target2","Project2.Release+Target", "Project2.Release+Target2"}},

    // negative tests
    { {"Unknown"},                       {"Unknown"}, {}},
    { {".UnknownBuild"},                 {".UnknownBuild"}, {}},
    { {"+UnknownTarget"},                {"+UnknownTarget"}, {}},
    { {"Project.UnknownBuild"},          {"Project.UnknownBuild"}, {}},
    { {"Project+UnknownTarget"},         {"Project+UnknownTarget"}, {}},
    { {".UnknownBuild+Target"},          {".UnknownBuild+Target"}, {}},
    { {"TestProject*"},                  {"TestProject*"}, {}},
    { {"Project.*Build"},                {"Project.*Build"}, {}},
    { {"Project.Debug+*H"},              {"Project.Debug+*H"}, {}},
    { {"Project1.Release.Debug+Target"}, {"Project1.Release.Debug+Target"}, {}},
    { {"Project1.Debug+Target+Target2"}, {"Project1.Debug+Target+Target2"}, {}},
  };

  list<string> selectedContexts;
  for (const auto& [contextFilters, expectedRetval, expectedContexts] : vecTestData) {
    string input;
    selectedContexts.clear();
    std::for_each(contextFilters.begin(), contextFilters.end(),
      [&](const std::string& item) { input += item + " "; });
    EXPECT_EQ(expectedRetval, GetSelectedContexts(selectedContexts, allContexts, contextFilters)) <<
      "failed for input \"" << input << "\"";
    ASSERT_EQ(selectedContexts.size(), expectedContexts.size());
    EXPECT_EQ(selectedContexts, expectedContexts);
  }
}
