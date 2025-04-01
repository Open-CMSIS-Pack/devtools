/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "RteModelTestConfig.h"

#include "RteModel.h"
#include "RteKernelSlim.h"
#include "RteItemBuilder.h"

#include "XMLTree.h"
#include "XmlFormatter.h"

#include "XMLTreeSlim.h"

#include "RteFsUtils.h"

using namespace std;

class RteExampleTest :public ::testing::Test {
public:
  static RteGlobalModel* s_model;

protected:
  void SetUp() override;
  void TearDown() override;
};

RteGlobalModel* RteExampleTest::s_model = nullptr;

void RteExampleTest::SetUp() {
  list<string> files;
  RteFsUtils::GetPackageDescriptionFiles(files, RteModelTestConfig::CMSIS_PACK_ROOT, 3);
  ASSERT_TRUE(files.size() > 0);

  s_model = new RteGlobalModel();
  s_model->SetUseDeviceTree(true);
  RteKernelSlim rteKernel(s_model);  // here just to instantiate XMLTree parser and read packs, model will persist

  list<RtePackage*> packs;
  ASSERT_TRUE(rteKernel.LoadPacks(files, packs));
  ASSERT_TRUE(!packs.empty());
  s_model->InsertPacks(packs);

  bool rteModelValidateResult = s_model->Validate();
  ASSERT_TRUE(rteModelValidateResult);
}


void RteExampleTest::TearDown() {
  if (RteExampleTest::s_model) {
    delete RteExampleTest::s_model;
    RteExampleTest::s_model = nullptr;
  }
}


TEST_F(RteExampleTest, TestExamplePaths) {
  const string testPackID = "ARM::RteTest@0.1.0";

  // Expected results
  const int exampleCountExp = 2;
  // Common expected
  const string loadPathExp = "PreInclude.uvprojx";
  // PreInclude example - expected
  const string preInclude_FolderExp = "Examples/PreInclude";
  const string preInclude_EnvFolderExp = "";
  // PreIncludeWithEnvFolder example - expected
  const string preIncludeEnv_FolderExp = "Examples";
  const string preIncludeEnv_EnvFolderExp = "PreInclude";


  RteModel *model = RteExampleTest::s_model;
  RtePackage *package = model->GetPackage(testPackID);
  ASSERT_NE(package, nullptr);

  const int exampleCount = package->GetExampleCount();
  EXPECT_EQ(exampleCount, exampleCountExp);

  RteItem *examples = package->GetExamples();
  ASSERT_NE(examples, nullptr);

  auto exampleList = examples->GetChildren();
  EXPECT_EQ(exampleCount, exampleList.size());

  RteExample *preIncludeExample = nullptr;
  RteExample *preIncludeEnvFolderExample = nullptr;

  for (auto exampleIt = exampleList.begin(); exampleIt != exampleList.end(); exampleIt++) {
    RteExample *example = dynamic_cast<RteExample*>(*exampleIt);
    if (example == nullptr) {
      break;
    }
    if (example->GetName() == "PreInclude") {
      preIncludeExample = example;
    } else if (example->GetName() == "PreIncludeEnvFolder") {
      preIncludeEnvFolderExample = example;
    }
  }

  ASSERT_NE(preIncludeExample, nullptr);
  EXPECT_STREQ(preIncludeExample->GetAttribute("folder").c_str(), preInclude_FolderExp.c_str());
  EXPECT_STREQ(preIncludeExample->GetEnvironmentAttribute("uv", "folder").c_str(), preInclude_EnvFolderExp.c_str());
  EXPECT_STREQ(preIncludeExample->GetLoadPath("uv").c_str(), loadPathExp.c_str());

  ASSERT_NE(preIncludeEnvFolderExample, nullptr);
  EXPECT_STREQ(preIncludeEnvFolderExample->GetAttribute("folder").c_str(), preIncludeEnv_FolderExp.c_str());
  EXPECT_STREQ(preIncludeEnvFolderExample->GetEnvironmentAttribute("uv", "folder").c_str(), preIncludeEnv_EnvFolderExp.c_str());
  EXPECT_STREQ(preIncludeEnvFolderExample->GetLoadPath("uv").c_str(), loadPathExp.c_str());
}