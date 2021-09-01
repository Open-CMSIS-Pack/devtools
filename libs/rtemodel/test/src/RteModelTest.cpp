/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "gtest/gtest.h"

#include "RteModelTestConfig.h"

#include "RteModel.h"
#include "RteKernelSlim.h"
#include "RteCprjProject.h"
#include "CprjFile.h"

#include "XMLTree.h"
#include "XmlFormatter.h"

#include "XMLTreeSlim.h"

#include "RteFsUtils.h"

#include <iostream>
#include <fstream>
#include <unordered_map>

using namespace std;

TEST(RteModelTest, LoadPacks) {

  RteKernelSlim rteKernel;  // here just to instantiate XMLTree parser
  list<string> files;
  RteFsUtils::GetPackageDescriptionFiles(files, RteModelTestConfig::CMSIS_PACK_ROOT, 3);
  EXPECT_TRUE(files.size() > 0);
  unique_ptr<XMLTree> xmlTree = rteKernel.CreateUniqueXmlTree();
  xmlTree->SetFileNames(files, false);
  xmlTree->Init();

  bool pdscParseResult = xmlTree->ParseAll();
  EXPECT_TRUE(pdscParseResult);

  RteModel rteModel;
  bool rteModelConstructResult = rteModel.Construct(xmlTree.get());
  rteModel.SetUseDeviceTree(true);
  EXPECT_TRUE(rteModelConstructResult);

  bool rteModelValidateResult = rteModel.Validate();
  EXPECT_TRUE(rteModelValidateResult);

}

// define project and header file names with relative paths
const string prjsDir = "RteModelTestProjects";
const string RteTestM3 = "/RteTestM3";
const string RteTestM3_cprj = prjsDir + RteTestM3 + "/RteTestM3.cprj";

const string preIncComp = prjsDir + RteTestM3 + "/RTE/_RteTestM3/Pre_Include__RteTest_ComponentLevel.h";
const string preIncGlob = prjsDir + RteTestM3 + "/RTE/_RteTestM3/Pre_Include_Global.h";
const string rteComp = prjsDir + RteTestM3 + "/RTE/_RteTestM3/RTE_Components.h";

const string RteTestM4 = "/RteTestM4";
const string RteTestM4_cprj = prjsDir + RteTestM4 + "/RteTestM4.cprj";
const string RteTestM4_CompDep_cprj = prjsDir + RteTestM4 + "/RteTestM4_CompDep.cprj";



class RteModelPrjTest :public ::testing::Test {
public:

protected:
  void SetUp() override
  {
    RteFsUtils::DeleteTree(prjsDir);
    RteFsUtils::CopyTree(RteModelTestConfig::PROJECTS_DIR, prjsDir);
  }

  void TearDown() override
  {
    RteFsUtils::DeleteTree(prjsDir);
  }

  void compareFile(const string &newFile, const string &refFile,
    const std::unordered_map<string, string> &expectedChangedFlags, const string &toolchain) const;
};

TEST_F(RteModelPrjTest, LoadCprj) {

  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);

  // check if active project is set
  RteCprjProject* activeCprjProject = rteKernel.GetActiveCprjProject();
  ASSERT_NE(activeCprjProject, nullptr);
  // check if it is the loaded project
  EXPECT_EQ(activeCprjProject, loadedCprjProject);

  RteDeviceItem* device = rteKernel.GetActiveDevice();
  string deviceName = device ? device->GetName() : RteUtils::ERROR_STRING;

  EXPECT_EQ(deviceName, "RteTest_ARMCM3");

  RteTarget* activeTarget = activeCprjProject->GetActiveTarget();
  ASSERT_NE(activeTarget, nullptr);
  map<const RteItem*, RteDependencyResult> depResults;
  RteItem::ConditionResult res = activeTarget->GetDepsResult(depResults, activeTarget);
  EXPECT_EQ(res, RteItem::FULFILLED);
}

TEST_F(RteModelPrjTest, GenerateHeaderTest)
{
  // backup header files into buffers
  string preIncCompBuf;
  string preIncGlobBuf;
  string rteCompBuf;
  RteFsUtils::ReadFile(preIncComp, preIncCompBuf);
  RteFsUtils::ReadFile(preIncGlob, preIncGlobBuf);
  RteFsUtils::ReadFile(rteComp, rteCompBuf);

  // remove header files
  RteFsUtils::DeleteFileAutoRetry(preIncComp);
  RteFsUtils::DeleteFileAutoRetry(preIncGlob);
  RteFsUtils::DeleteFileAutoRetry(rteComp);

  // load cprj test project
  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);

  // check if active project is set
  RteCprjProject* activeCprjProject = rteKernel.GetActiveCprjProject();
  ASSERT_NE(activeCprjProject, nullptr);

  // check if it is the loaded project
  EXPECT_EQ(activeCprjProject, loadedCprjProject);

  // check if device name is set
  RteDeviceItem* device = rteKernel.GetActiveDevice();
  string deviceName = device ? device->GetName() : RteUtils::ERROR_STRING;
  EXPECT_EQ(deviceName, "RteTest_ARMCM3");

  // check if header files are generated
  error_code ec;
  EXPECT_TRUE(fs::exists(preIncComp, ec));
  EXPECT_TRUE(fs::exists(preIncGlob, ec));
  EXPECT_TRUE(fs::exists(rteComp, ec));

  // check if contents of header files are identical
  EXPECT_TRUE(RteFsUtils::CmpFileMem(preIncComp, preIncCompBuf));
  EXPECT_TRUE(RteFsUtils::CmpFileMem(preIncGlob, preIncGlobBuf));
  EXPECT_TRUE(RteFsUtils::CmpFileMem(rteComp, rteCompBuf));

  // reload project and check if timestamps are preserved
  auto timestampPreIncComp = fs::last_write_time(preIncComp, ec);
  auto timestamppreIncGlob = fs::last_write_time(preIncGlob, ec);
  auto timestamprteComp = fs::last_write_time(rteComp, ec);
  loadedCprjProject = rteKernel.LoadCprj(RteTestM3_cprj);
  EXPECT_TRUE(loadedCprjProject != nullptr);
  EXPECT_TRUE(timestampPreIncComp == fs::last_write_time(preIncComp, ec));
  EXPECT_TRUE(timestamppreIncGlob == fs::last_write_time(preIncGlob, ec));
  EXPECT_TRUE(timestamprteComp == fs::last_write_time(rteComp, ec));
}

TEST_F(RteModelPrjTest, LoadCprjCompDep) {

  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM4_CompDep_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);

  // check if active project is set
  RteCprjProject* activeCprjProject = rteKernel.GetActiveCprjProject();
  ASSERT_NE(activeCprjProject, nullptr);
  // check if it is the loaded project
  EXPECT_EQ(activeCprjProject, loadedCprjProject);

  RteDeviceItem* device = rteKernel.GetActiveDevice();
  string deviceName = device ? device->GetName() : RteUtils::ERROR_STRING;

  EXPECT_EQ(deviceName, "RteTest_ARMCM4_FP");

  RteTarget* activeTarget = activeCprjProject->GetActiveTarget();
  ASSERT_NE(activeTarget, nullptr);
  map<const RteItem*, RteDependencyResult> depResults;
  RteItem::ConditionResult res = activeTarget->GetDepsResult(depResults, activeTarget);
  EXPECT_EQ(res, RteItem::SELECTABLE);
}


#define CFLAGS "-xc -std=c99 --target=arm-arm-none-eabi -mcpu=cortex-m3"
#define CXXFLAGS "-cxx"
#define LDFLAGS "--cpu Cortex-M3"
#define ASFLAGS "--pd \"__MICROLIB SETA 1\" --xref -g"
#define ARFLAGS "-arflag"

TEST_F(RteModelPrjTest, GetTargetBuildFlags)
{
  // load cprj test project
  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);

  // check if active project is set
  RteCprjProject* activeCprjProject = rteKernel.GetActiveCprjProject();
  ASSERT_NE(activeCprjProject, nullptr);

  // check if it is the loaded project
  EXPECT_EQ(activeCprjProject, loadedCprjProject);
  string toolchain = activeCprjProject->GetToolchain();

  CprjFile* cprjFile = activeCprjProject->GetCprjFile();
  ASSERT_NE(cprjFile, nullptr);
  CprjTargetElement* te = cprjFile->GetTargetElement();
  ASSERT_NE(te, nullptr);

  XMLTreeSlim tree;
  EXPECT_TRUE(tree.ParseFile(RteTestM3_cprj));
  ASSERT_NE(tree.GetRoot(), nullptr);
  XMLTreeElement* root = tree.GetRoot()->GetFirstChild();
  ASSERT_NE(root, nullptr);

  auto target = root->GetGrandChildren("target");
  auto getflags = [&target](const string &tag) {
    for (auto item : target) {
      if (item->GetTag() == tag) {
        return item->GetAttribute("add");
      }
    }
    return RteUtils::EMPTY_STRING;
  };

  // test getter functions
  string cflags = getflags("cflags");
  string cxxflags = getflags("cxxflags");
  string ldflags = getflags("ldflags");
  string asflags = getflags("asflags");
  string arflags = getflags("arflags");
  EXPECT_TRUE(arflags.compare(te->GetArFlags(toolchain)) == 0);
  EXPECT_TRUE(cflags.compare(te->GetCFlags(toolchain)) == 0);
  EXPECT_TRUE(cxxflags.compare(te->GetCxxFlags(toolchain)) == 0);
  EXPECT_TRUE(ldflags.compare(te->GetLdFlags(toolchain)) == 0);
  EXPECT_TRUE(asflags.compare(te->GetAsFlags(toolchain)) == 0);
}

TEST_F(RteModelPrjTest, SetTargetBuildFlags)
{
  // load cprj test project
  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);

  // check if active project is set
  RteCprjProject* activeCprjProject = rteKernel.GetActiveCprjProject();
  ASSERT_NE(activeCprjProject, nullptr);

  // check if it is the loaded project
  EXPECT_EQ(activeCprjProject, loadedCprjProject);
  string toolchain = activeCprjProject->GetToolchain();

  CprjFile* cprjFile = activeCprjProject->GetCprjFile();
  ASSERT_NE(cprjFile, nullptr);
  CprjTargetElement* te = cprjFile->GetTargetElement();
  ASSERT_NE(te, nullptr);

  // test setter with attributes removed
  auto CheckAttributeRemoved = [&toolchain, te](const string &flags) {
    RteItem *item = te->GetChildByTagAndAttribute(flags, "compiler", toolchain);
    if (item) {
      EXPECT_FALSE(item->HasAttribute("add"));
    }
  };

  te->SetCFlags(RteUtils::EMPTY_STRING, toolchain);
  te->SetCxxFlags(RteUtils::EMPTY_STRING, toolchain);
  te->SetLdFlags(RteUtils::EMPTY_STRING, toolchain);
  te->SetAsFlags(RteUtils::EMPTY_STRING, toolchain);
  te->SetArFlags(RteUtils::EMPTY_STRING, toolchain);

  CheckAttributeRemoved("cflags");
  CheckAttributeRemoved("cxxflags");
  CheckAttributeRemoved("ldflags");
  CheckAttributeRemoved("asflags");
  CheckAttributeRemoved("arflags");

  // test setter functions with all attributes set
  te->SetCFlags(CFLAGS, toolchain);
  te->SetCxxFlags(CXXFLAGS, toolchain);
  te->SetLdFlags(LDFLAGS, toolchain);
  te->SetAsFlags(ASFLAGS, toolchain);
  te->SetArFlags(ARFLAGS, toolchain);

  EXPECT_TRUE(te->GetCFlags(toolchain).compare(CFLAGS) == 0);
  EXPECT_TRUE(te->GetCxxFlags(toolchain).compare(CXXFLAGS) == 0);
  EXPECT_TRUE(te->GetLdFlags(toolchain).compare(LDFLAGS) == 0);
  EXPECT_TRUE(te->GetAsFlags(toolchain).compare(ASFLAGS) == 0);
  EXPECT_TRUE(te->GetArFlags(toolchain).compare(ARFLAGS) == 0);
}

TEST_F(RteModelPrjTest, UpdateCprjFile)
{
  // load cprj test project
  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);

  // check if active project is set
  RteCprjProject* activeCprjProject = rteKernel.GetActiveCprjProject();
  ASSERT_NE(activeCprjProject, nullptr);

  // check if it is the loaded project
  EXPECT_EQ(activeCprjProject, loadedCprjProject);
  string toolchain = activeCprjProject->GetToolchain();

  CprjFile* cprjFile = activeCprjProject->GetCprjFile();
  ASSERT_NE(cprjFile, nullptr);
  CprjTargetElement* te = cprjFile->GetTargetElement();
  ASSERT_NE(te, nullptr);

  // test save active cprj file: 2 test cases
  rteKernel.SaveActiveCprjFile();
  const std::unordered_map<string, string> nothingChanged;
  const std::unordered_map<string, string> changedFlags = {
    { "<ldflags",  LDFLAGS },
    { "<cflags",   CFLAGS },
    { "<asflags",  ASFLAGS },
    { "<cxxflags", CXXFLAGS },
    { "<arflags",  ARFLAGS }
  };
  const string newFile = cprjFile->GetPackageFileName();
  const string refFile = RteModelTestConfig::PROJECTS_DIR + "/RteTestM3/RteTestM3.cprj";
  compareFile(newFile, refFile, nothingChanged, toolchain);  // expected: nothing changed
  te->SetCFlags(CFLAGS, toolchain);
  te->SetCxxFlags(CXXFLAGS, toolchain);
  te->SetLdFlags(LDFLAGS, toolchain);
  te->SetAsFlags(ASFLAGS, toolchain);
  te->SetArFlags(ARFLAGS, toolchain);
  rteKernel.SaveActiveCprjFile();
  compareFile(newFile, refFile, changedFlags, toolchain);    // expected: only build flags changed
}

void RteModelPrjTest::compareFile(const string &newFile, const string &refFile,
  const std::unordered_map<string, string> &expectedChangedFlags, const string &toolchain) const {
  ifstream streamNewFile, streamRefFile;
  string newLine, refLine;

  streamNewFile.open(newFile);
  EXPECT_TRUE(streamNewFile.is_open());

  streamRefFile.open(refFile);
  EXPECT_TRUE(streamRefFile.is_open());

  auto nextline = [](ifstream &f, string &line, bool wait)
  {
    if (wait)
      return true;
    line.clear();
    while (getline(f, line)) {
      if (!line.empty())
        return true;
    }
    return false;
  };

  auto compareLine = [](const string &key, const string &flags, const string &line)
  {
    string res;
    size_t pos, pos2 = string::npos;
    if (line.find(key) != string::npos) {
      pos = line.find("\"");
      if (pos != string::npos)
        pos2 = line.find("\"", pos + 1);
      if (pos != string::npos && pos2 != string::npos) {
        res = line;
        string convFlags = XmlFormatter::ConvertSpecialChars(flags);
        res.replace(pos + 1, pos2 - pos - 1, convFlags);
        EXPECT_EQ(res.compare(line), 0);
      }
    }
  };

  auto getTag = [&toolchain](const string &line) {
    string compiler = "compiler=\"" + toolchain + "\"";
    if (line.find(compiler) != string::npos) {
      size_t pos1 = line.find("<");
      if (pos1 != string::npos) {
        size_t pos2 = line.find(" ", pos1);
        if (pos2 != string::npos)
          return line.substr(pos1, pos2 - pos1);
      }
    }
    return RteUtils::EMPTY_STRING;
  };

  // compare lines of files
  bool wait = false;
  bool diff = false;
  while (nextline(streamNewFile, newLine, false) && nextline(streamRefFile, refLine, wait)) {
    if (newLine != refLine) {
      auto iter = expectedChangedFlags.find(getTag(newLine));
      if (iter != expectedChangedFlags.end()) {
        compareLine(iter->first, iter->second, newLine);
        if (!wait) {
          iter = expectedChangedFlags.find(getTag(refLine));
          if (iter == expectedChangedFlags.end())
            wait = true;      // wait until checking buildflags in updated file is done
        }
      }
      else {
        diff = true;
        break;
      }
    }
    else {
      wait = false;
    }
  }

  streamNewFile.close();
  streamRefFile.close();
  if (diff) {
    FAIL() << newFile << " is different from " << refFile;
  }
}
