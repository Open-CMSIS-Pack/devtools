/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "ProjMgrUtils.h"
#include "RteFsUtils.h"
#include "RteItem.h"
#include "gtest/gtest.h"

using namespace std;

class ProjMgUtilsUnitTests : public ProjMgrUtils, public ::testing::Test {
protected:
  ProjMgUtilsUnitTests() {}
  virtual ~ProjMgUtilsUnitTests() {}
};

TEST_F(ProjMgUtilsUnitTests, GetComponentID) {
  const map<string, string> attributes = {
    {"Cvendor" , "Vendor"  },
    {"Cclass"  , "Class"   },
    {"Cbundle" , "Bundle"  },
    {"Cgroup"  , "Group"   },
    {"Csub"    , "Sub"     },
    {"Cvariant", "Variant" },
    {"Cversion", "9.9.9"   }
  };
  RteAttributes item = RteAttributes(attributes);
  EXPECT_EQ("Vendor::Class&Bundle:Group:Sub&Variant@9.9.9",GetComponentID((RteItem*)&item));
}

TEST_F(ProjMgUtilsUnitTests, GetComponentAggregateID) {
  const map<string, string> attributes = {
    {"Cvendor" , "Vendor"  },
    {"Cclass"  , "Class"   },
    {"Cbundle" , "Bundle"  },
    {"Cgroup"  , "Group"   },
    {"Csub"    , "Sub"     },
    {"Cvariant", "Variant" },
    {"Cversion", "9.9.9"   }
  };
  RteAttributes item = RteAttributes(attributes);
  EXPECT_EQ("Vendor::Class&Bundle:Group:Sub", GetComponentAggregateID((RteItem*)&item));
}

TEST_F(ProjMgUtilsUnitTests, GetConditionID) {
  const map<string, string> attributes = {
    {"Cvendor" , "Vendor"  },
    {"Cclass"  , "Class"   },
    {"Cgroup"  , "Group"   }
  };
  RteAttributes item = RteAttributes(attributes);
  item.SetTag("require");
  EXPECT_EQ("require Vendor::Class:Group", GetConditionID((RteItem*)&item));
}

TEST_F(ProjMgUtilsUnitTests, GetPackageID) {
  const map<string, string> attributes = {
    {"vendor" , "Vendor"  },
    {"name"   , "Name"   },
    {"version", "8.8.8"   }
  };
  RteAttributes item = RteAttributes(attributes);
  item.SetTag("require");
  EXPECT_EQ("Vendor::Name@8.8.8", GetPackageID((RteItem*)&item));
}

TEST_F(ProjMgUtilsUnitTests, ReadGpdscFile) {
  unique_ptr<RteGeneratorModel> gpdscModel = make_unique<RteGeneratorModel>();
  const string& gpdscFile = testinput_folder + "/TestGenerator/RTE/Device/RteTestGen_ARMCM0/RteTest.gpdsc";
  EXPECT_EQ(true, ProjMgrUtils::ReadGpdscFile(gpdscFile, gpdscModel.get()));
  EXPECT_EQ("RteTestGeneratorIdentifier", gpdscModel.get()->GetGeneratorName());
  gpdscModel.reset();
}

TEST_F(ProjMgUtilsUnitTests, ExecCommand) {
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
