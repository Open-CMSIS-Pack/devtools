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
  EXPECT_TRUE(compare(ParseContextEntry("project"             ), { "project", ""     , ""       }));
  EXPECT_TRUE(compare(ParseContextEntry("project.build"       ), { "project", "build", ""       }));
  EXPECT_TRUE(compare(ParseContextEntry("project+target"      ), { "project", ""     , "target" }));
  EXPECT_TRUE(compare(ParseContextEntry("project.build+target"), { "project", "build", "target" }));
  EXPECT_TRUE(compare(ParseContextEntry("project+target.build"), { "project", "build", "target" }));
  EXPECT_TRUE(compare(ParseContextEntry(".build"              ), { ""       , "build", ""       }));
  EXPECT_TRUE(compare(ParseContextEntry(".build+target"       ), { ""       , "build", "target" }));
  EXPECT_TRUE(compare(ParseContextEntry("+target"             ), { ""       , ""     , "target" }));
  EXPECT_TRUE(compare(ParseContextEntry("+target.build"       ), { ""       , "build", "target" }));
  EXPECT_TRUE(compare(ParseContextEntry(""                    ), { ""       , ""     , ""       }));
};
