/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
  rteModel.SetUseDeviceTree(true);
  bool rteModelConstructResult = rteModel.Construct(xmlTree.get());
  EXPECT_TRUE(rteModelConstructResult);

  bool rteModelValidateResult = rteModel.Validate();
  EXPECT_TRUE(rteModelValidateResult);

  RteDeviceItemAggregate* da = rteModel.GetDeviceAggregate("RteTest_ARMCM3", "ARM:82");
  ASSERT_NE(da, nullptr);
  // test deprecated memory attributes: IROM and IRAM
  string summary = da->GetSummaryString();
  EXPECT_EQ(summary, "ARM Cortex-M3, 10 MHz, 128 kB RAM, 256 kB ROM");

  da = rteModel.GetDeviceAggregate("RteTest_ARMCM4", "ARM:82");
  ASSERT_NE(da, nullptr);
  // test recommended memory attributes: name and access
  summary = da->GetSummaryString();
  EXPECT_EQ(summary, "ARM Cortex-M4, 10 MHz, 128 kB RAM, 256 kB ROM");

  RteBoard* board = rteModel.FindBoard("RteTest board listing (Rev.C)");
  ASSERT_NE(board, nullptr);
  EXPECT_TRUE(board->HasMCU());
  list<RteItem*> algos;
  EXPECT_EQ(board->GetAlgorithms(algos).size(), 2);
  list<RteItem*> mems;
  EXPECT_EQ(board->GetMemories(mems).size(), 2);

  board = rteModel.FindBoard("RteTest NoMCU board");
  ASSERT_NE(board, nullptr);
  EXPECT_FALSE(board->HasMCU());
  algos.clear();
  EXPECT_EQ(board->GetAlgorithms(algos).size(), 0);
  mems.clear();
  EXPECT_EQ(board->GetMemories(mems).size(), 2);
}

class RteModelPrjTest : public RteModelTestConfig {
public:

protected:
  void compareFile(const string &newFile, const string &refFile,
    const std::unordered_map<string, string> &expectedChangedFlags, const string &toolchain) const;

  string UpdateLocalIndex() {
    const string index = localRepoDir + "/.Local/local_repository.pidx";
    const string pdsc = RteModelTestConfig::CMSIS_PACK_ROOT + "/ARM/RteTest/0.1.0/ARM.RteTest.pdsc";
    const string original = "file://localhost/packs/LocalVendor/LocalPack/";
    const string replace = "file://localhost/" + RteModelTestConfig::CMSIS_PACK_ROOT + "/ARM/RteTest/0.1.0/";
    string line;
    vector<string> buffer;

    ifstream in(index);
    while (getline(in, line)) {
      size_t pos = line.find(original);
      if (pos != string::npos) {
        line.replace(pos, original.length(), replace);
      }
      buffer.push_back(line);
    }
    in.close();

    ofstream out(index);
    for (vector<string>::iterator it = buffer.begin(); it != buffer.end(); it++) {
      out << *it << endl;
    }
    return pdsc;
  }

  void GenerateHeadersTest(const string& project, const string& rteFolder) {

    const string projectDir = RteUtils::ExtractFilePath(project, true);
    const string targetFolder = "/_Target_1/";
    const string preIncComp = projectDir + rteFolder + targetFolder + "Pre_Include_RteTest_ComponentLevel.h";
    const string preIncGlob = projectDir + rteFolder + targetFolder + "Pre_Include_Global.h";
    const string rteComp = projectDir + rteFolder + targetFolder + "RTE_Components.h";

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
    RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(project);
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
  string deviceVendor = device ? device->GetVendorString() : RteUtils::ERROR_STRING;
  EXPECT_EQ(deviceName, "RteTest_ARMCM3");

  RteTarget* activeTarget = activeCprjProject->GetActiveTarget();
  ASSERT_NE(activeTarget, nullptr);
  map<const RteItem*, RteDependencyResult> depResults;
  RteItem::ConditionResult res = activeTarget->GetDepsResult(depResults, activeTarget);
  EXPECT_EQ(res, RteItem::FULFILLED);
  string boardName = activeTarget->GetAttribute("Bname");
  EXPECT_EQ(boardName, "RteTest Test board");
  // get layers
  auto& allLayerDescriptors = rteKernel.GetGlobalModel()->GetLayerDescriptors();
  EXPECT_EQ(allLayerDescriptors.size(), 8);
  auto& filteredLayerDescriptors = activeTarget->GetFilteredModel()->GetLayerDescriptors();
  EXPECT_EQ(filteredLayerDescriptors.size(), 6);

  const string rteDir = RteUtils::ExtractFilePath(RteTestM3_cprj, true) + "RTE/";
  const string CompConfig_0_Base_Version = rteDir + "RteTest/" + "ComponentLevelConfig_0.h.base@0.0.1";
  const string CompConfig_1_Base_Version = rteDir + "RteTest/" + "ComponentLevelConfig_1.h.base@0.0.1";
  EXPECT_TRUE(RteFsUtils::Exists(CompConfig_0_Base_Version));
  EXPECT_TRUE(RteFsUtils::Exists(CompConfig_1_Base_Version));

  error_code ec;
  const fs::perms write_mask = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;
  // check config file PLM: existence and permissions
  const string deviceDir = rteDir + "Device/RteTest_ARMCM3/";
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "ARMCM3_ac6.sct.base@1.0.0"));
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "ARMCM3_ac6.sct.update@1.2.0"));

  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "system_ARMCM3.c.base@1.0.1"));
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "system_ARMCM3.c.base@1.0.2"));

  EXPECT_TRUE(RteFsUtils::Exists(deviceDir + "startup_ARMCM3.c.base@2.0.3"));
  EXPECT_EQ((fs::status(deviceDir + "startup_ARMCM3.c.base@2.0.3", ec).permissions() & write_mask), fs::perms::none);

  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "system_ARMCM3.c.update@1.2.2"));
}

TEST_F(RteModelPrjTest, LoadCprj_NoRTEFileCreation) {

  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_cprj, RteUtils::EMPTY_STRING, true, false);
  ASSERT_NE(loadedCprjProject, nullptr);

  const string rteDir = RteUtils::ExtractFilePath(RteTestM3_cprj, true) + "RTE/";
  const string CompConfig_0_Base_Version = rteDir + "RteTest/" + "ComponentLevelConfig_0.h.base@0.0.1";
  const string CompConfig_1_Base_Version = rteDir + "RteTest/" + "ComponentLevelConfig_1.h.base@0.0.1";
  EXPECT_FALSE(RteFsUtils::Exists(CompConfig_0_Base_Version));
  EXPECT_FALSE(RteFsUtils::Exists(CompConfig_1_Base_Version));

  const string deviceDir = rteDir + "Device/RteTest_ARMCM3/";
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "ARMCM3_ac6.sct.update@1.2.0"));
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "startup_ARMCM3.c.base@2.0.3"));
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "system_ARMCM3.c.update@1.2.2"));

  // additionally test support for RTE folder with spaces
  auto& fileInstances= loadedCprjProject->GetFileInstances();
  // take existing file instance
  auto it = fileInstances.find("RTE/Device/RteTest_ARMCM3/startup_ARMCM3.c");
  RteFileInstance* fi = it != fileInstances.end() ? it->second : nullptr;
  ASSERT_NE(fi, nullptr);
  // use its file to create path with another RTE directory
  RteFile* f = fi ? fi->GetFile(loadedCprjProject->GetActiveTargetName()) : nullptr ;
  ASSERT_NE(f, nullptr);
  const string& deviceName = loadedCprjProject->GetActiveTarget()->GetDeviceName();
  string pathName = f->GetInstancePathName(deviceName, 0 , "RTE With Spaces");
  EXPECT_EQ(pathName, "RTE With Spaces/Device/RteTest_ARMCM3/startup_ARMCM3.c");
}

TEST_F(RteModelPrjTest, LoadCprj_PackPath) {

  RteFsUtils::CopyTree(RteModelTestConfig::CMSIS_PACK_ROOT, RteTestM3_PrjPackPath);

  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot("dummy");
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_PackPath_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);

  // check if active project is set
  RteCprjProject* activeCprjProject = rteKernel.GetActiveCprjProject();
  ASSERT_NE(activeCprjProject, nullptr);
  // check if it is the loaded project
  EXPECT_EQ(activeCprjProject, loadedCprjProject);

  RteDeviceItem* device = rteKernel.GetActiveDevice();
  string deviceName = device ? device->GetName() : RteUtils::ERROR_STRING;
  string deviceVendor = device ? device->GetVendorString() : RteUtils::ERROR_STRING;
  EXPECT_EQ(deviceName, "RteTest_ARMCM3");

  RteTarget* activeTarget = activeCprjProject->GetActiveTarget();
  ASSERT_NE(activeTarget, nullptr);
  map<const RteItem*, RteDependencyResult> depResults;
  RteItem::ConditionResult res = activeTarget->GetDepsResult(depResults, activeTarget);
  EXPECT_EQ(res, RteItem::FULFILLED);

  RteFsUtils::DeleteTree(RteTestM3_PrjPackPath);
}

TEST_F(RteModelPrjTest, LoadCprj_PackPath_MultiplePdscs) {

  RteFsUtils::CopyTree(RteModelTestConfig::CMSIS_PACK_ROOT, RteTestM3_PrjPackPath);

  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot("dummy");
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_PackPath_MultiplePdscs_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);
  EXPECT_EQ(loadedCprjProject->GetFilteredPacks().size(), 0);

  RteFsUtils::DeleteTree(RteTestM3_PrjPackPath);
}

TEST_F(RteModelPrjTest, LoadCprj_PackPath_NoPdsc) {
  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot("dummy");
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_PackPath_NoPdsc_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);
  EXPECT_EQ(loadedCprjProject->GetFilteredPacks().size(), 0);
}

TEST_F(RteModelPrjTest, LoadCprj_PackPath_Invalid) {
  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot("dummy");
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_PackPath_Invalid_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);
  EXPECT_EQ(loadedCprjProject->GetFilteredPacks().size(), 0);
}

TEST_F(RteModelPrjTest, LoadCprjConfigVer) {

  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_ConfigFolder_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);

  const string rteDir = RteUtils::ExtractFilePath(RteTestM3_cprj, true) + loadedCprjProject->GetRteFolder() + "/";
  const string CompConfig_0_Base_Version = rteDir + "RteTest/" + "ComponentLevelConfig_0.h.base@0.0.1";
  const string CompConfig_1_Base_Version = rteDir + "RteTest/" + "ComponentLevelConfig_1.h.base@0.0.1";
  EXPECT_TRUE(RteFsUtils::Exists(CompConfig_0_Base_Version));
  EXPECT_TRUE(RteFsUtils::Exists(CompConfig_1_Base_Version));

  const string deviceDir = rteDir + "Device/RteTest_ARMCM3/";
  EXPECT_TRUE(RteFsUtils::Exists(deviceDir + "ARMCM3_ac6.sct"));
  EXPECT_TRUE(RteFsUtils::Exists(deviceDir + "ARMCM3_ac6.sct.base@1.0.0"));
  // check if file version is taken from  base file (project contains "5.5.5")
  RteFileInstance* fi = loadedCprjProject->GetFileInstance("CONFIG_FOLDER/Device/RteTest_ARMCM3/ARMCM3_ac6.sct");
  EXPECT_TRUE(fi && fi->GetVersionString() == "1.0.0");

  EXPECT_TRUE(RteFsUtils::Exists(deviceDir + "startup_ARMCM3.c.base@2.0.3"));
  EXPECT_TRUE(RteFsUtils::Exists(deviceDir + "system_ARMCM3.c.base@1.0.1"));
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "system_ARMCM3.c.base@1.0.2"));
  EXPECT_TRUE(RteFsUtils::Exists(deviceDir + "system_ARMCM3.c.update@1.2.2"));

  const string depsDir = rteDir + "Dependency/RteTest_ARMCM3/";
  EXPECT_TRUE(RteFsUtils::Exists(depsDir + "DeviceDependency.c.base@1.1.1"));
  EXPECT_TRUE(RteFsUtils::Exists(depsDir + "DeviceDependency.c"));
  EXPECT_TRUE(RteFsUtils::Exists(depsDir + "BoardDependency.c.base@1.2.2"));
  EXPECT_TRUE(RteFsUtils::Exists(depsDir + "BoardDependency.c"));

  // update file version
  fi = loadedCprjProject->GetFileInstance("CONFIG_FOLDER/Device/RteTest_ARMCM3/system_ARMCM3.c");
  EXPECT_TRUE(fi && fi->GetVersionString() == "1.0.1");
  const string& targetName = loadedCprjProject->GetActiveTargetName();
  RteFile* f = fi->GetFile(targetName);
  EXPECT_TRUE(loadedCprjProject->UpdateFileToNewVersion(fi, f, true));
  EXPECT_TRUE(fi && fi->GetVersionString() == "1.2.2");
  // check if backups and new version files have been created
  EXPECT_TRUE(RteFsUtils::Exists(deviceDir + "system_ARMCM3.c.0000"));
  EXPECT_TRUE(RteFsUtils::Exists(deviceDir + "system_ARMCM3.c.0000.base@1.0.1"));

  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "system_ARMCM3.c.base@1.0.1"));
  EXPECT_TRUE(RteFsUtils::Exists(deviceDir + "system_ARMCM3.c.base@1.2.2"));
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "system_ARMCM3.c.update@1.2.2"));

}

TEST_F(RteModelPrjTest, GetLocalPdscFile) {
  RteKernelSlim rteKernel;
  const string& expectedPdsc = UpdateLocalIndex();

  XmlItem attributes;
  attributes.AddAttribute("name", "LocalPack");
  attributes.AddAttribute("vendor", "LocalVendor");
  attributes.AddAttribute("version", "0.1.0");
  string packId;
  string pdsc = rteKernel.GetLocalPdscFile(attributes, localRepoDir, packId);

  // check returned packId
  EXPECT_EQ(packId, "LocalVendor.LocalPack.0.1.0");

  // check returned pdsc
  error_code ec;
  EXPECT_TRUE(fs::equivalent(pdsc, expectedPdsc, ec));
}

TEST_F(RteModelPrjTest, GenerateHeadersTestDefault)
{
  GenerateHeadersTest(RteTestM3_cprj, "RTE");
}

TEST_F(RteModelPrjTest, GenerateHeadersTest_ConfigFolder)
{
  GenerateHeadersTest(RteTestM3_ConfigFolder_cprj, "CONFIG_FOLDER");
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
#define LDCFLAGS "-lm"
#define LDCXXFLAGS "-lstdc++"
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
  string ldcflags = getflags("ldcflags");
  string ldcxxflags = getflags("ldcxxflags");
  string asflags = getflags("asflags");
  string arflags = getflags("arflags");
  EXPECT_TRUE(arflags.compare(te->GetArFlags(toolchain)) == 0);
  EXPECT_TRUE(cflags.compare(te->GetCFlags(toolchain)) == 0);
  EXPECT_TRUE(cxxflags.compare(te->GetCxxFlags(toolchain)) == 0);
  EXPECT_TRUE(ldflags.compare(te->GetLdFlags(toolchain)) == 0);
  EXPECT_TRUE(ldcflags.compare(te->GetLdCFlags(toolchain)) == 0);
  EXPECT_TRUE(ldcxxflags.compare(te->GetLdCxxFlags(toolchain)) == 0);
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
  te->SetLdCFlags(RteUtils::EMPTY_STRING, toolchain);
  te->SetLdCxxFlags(RteUtils::EMPTY_STRING, toolchain);
  te->SetAsFlags(RteUtils::EMPTY_STRING, toolchain);
  te->SetArFlags(RteUtils::EMPTY_STRING, toolchain);

  CheckAttributeRemoved("cflags");
  CheckAttributeRemoved("cxxflags");
  CheckAttributeRemoved("ldflags");
  CheckAttributeRemoved("ldcflags");
  CheckAttributeRemoved("ldcxxflags");
  CheckAttributeRemoved("asflags");
  CheckAttributeRemoved("arflags");

  // test setter functions with all attributes set
  te->SetCFlags(CFLAGS, toolchain);
  te->SetCxxFlags(CXXFLAGS, toolchain);
  te->SetLdFlags(LDFLAGS, toolchain);
  te->SetLdCFlags(LDCFLAGS, toolchain);
  te->SetLdCxxFlags(LDCXXFLAGS, toolchain);
  te->SetAsFlags(ASFLAGS, toolchain);
  te->SetArFlags(ARFLAGS, toolchain);

  EXPECT_TRUE(te->GetCFlags(toolchain).compare(CFLAGS) == 0);
  EXPECT_TRUE(te->GetCxxFlags(toolchain).compare(CXXFLAGS) == 0);
  EXPECT_TRUE(te->GetLdFlags(toolchain).compare(LDFLAGS) == 0);
  EXPECT_TRUE(te->GetLdCFlags(toolchain).compare(LDCFLAGS) == 0);
  EXPECT_TRUE(te->GetLdCxxFlags(toolchain).compare(LDCXXFLAGS) == 0);
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
    { "<ldflags",    LDFLAGS },
    { "<ldcflags",   LDCFLAGS },
    { "<ldcxxflags", LDCXXFLAGS },
    { "<cflags",     CFLAGS },
    { "<asflags",    ASFLAGS },
    { "<cxxflags",   CXXFLAGS },
    { "<arflags",    ARFLAGS }
  };
  const string newFile = cprjFile->GetPackageFileName();
  const string refFile = RteModelTestConfig::PROJECTS_DIR + "/RteTestM3/RteTestM3.cprj";
  compareFile(newFile, refFile, nothingChanged, toolchain);  // expected: nothing changed
  te->SetCFlags(CFLAGS, toolchain);
  te->SetCxxFlags(CXXFLAGS, toolchain);
  te->SetLdFlags(LDFLAGS, toolchain);
  te->SetLdCFlags(LDCFLAGS, toolchain);
  te->SetLdCxxFlags(LDCXXFLAGS, toolchain);
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

TEST_F(RteModelPrjTest, GetChildAttribute) {
  RteItem fileItem(nullptr);
  fileItem.SetTag("file");
  RteItem* optionsItem = fileItem.CreateChild("options");
  optionsItem->SetTag("options");
  optionsItem->SetAttribute("optimize", "size");

  string valid = fileItem.GetChildAttribute("options", "optimize");;
  string attr_invalid = fileItem.GetChildAttribute("options", "invalid");
  string tag_invalid = fileItem.GetChildAttribute("invalid", "whatever");

  EXPECT_EQ("size", valid);
  EXPECT_EQ("", attr_invalid);
  EXPECT_EQ("", tag_invalid);
}
