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

using namespace std;

TEST(RteModelTest, PackRegistry) {

  // tests for pack registry
  RteKernelSlim rteKernel;  // here just to instantiate XMLTree parser

  RtePackRegistry* packRegistry = rteKernel.GetPackRegistry();
  ASSERT_TRUE(packRegistry != nullptr);
  RteModel testModel(PackageState::PS_AVAILABLE);

  RtePackage* pack = new RtePackage(&testModel);
  pack->SetAttribute("name", "foo");
  pack->SetRootFileName("foo");
  EXPECT_TRUE(packRegistry->AddPack(pack));
  EXPECT_FALSE(packRegistry->AddPack(pack)); // not second time
  EXPECT_EQ(packRegistry->GetPack("foo"), pack);
  pack = new RtePackage(&testModel);
  pack->SetAttribute("name", "bar");
  pack->SetRootFileName("foo");
  EXPECT_TRUE(packRegistry->AddPack(pack, true));
  EXPECT_EQ(packRegistry->GetPack("foo"), pack);
  EXPECT_EQ(packRegistry->GetLoadedPacks().size(), 1);

  EXPECT_TRUE(packRegistry->ErasePack("foo"));
  EXPECT_EQ(packRegistry->GetPack("foo"), nullptr);
  EXPECT_FALSE(packRegistry->ErasePack("foo")); // already erased
  EXPECT_EQ(packRegistry->GetLoadedPacks().size(), 0);

}

TEST(RteModelTest, PackRegistryLoadPacks) {

  // tests for pack registry
  RteKernelSlim rteKernel;  // here just to instantiate XMLTree parser
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);

  RtePackRegistry* packRegistry = rteKernel.GetPackRegistry();
  ASSERT_TRUE(packRegistry != nullptr);
  RteModel testModel(PackageState::PS_INSTALLED);

  list<string> files;
  rteKernel.GetEffectivePdscFiles(files, false);
  EXPECT_FALSE(files.empty());
  list<RtePackage*> packs;
  EXPECT_TRUE(rteKernel.LoadPacks(files, packs, &testModel));
  EXPECT_FALSE(packs.empty());
  EXPECT_EQ(packs.size(), files.size());
  EXPECT_EQ(packRegistry->GetLoadedPacks().size(), packs.size());
 // to check if packs are the same or reloaded, modify the first pack
  ASSERT_NE(packs.begin(), packs.end());
  RtePackage* pack = *(packs.begin());
  ASSERT_TRUE(pack != nullptr);
  RteItem* dummyChild = new RteItem("dummy_child", pack);
  pack->AddItem(dummyChild);
  // no reload of the same files by default
  list<RtePackage*> packs1;
  EXPECT_TRUE(rteKernel.LoadPacks(files, packs1, &testModel));
  EXPECT_EQ(packs1.size(), files.size());
  EXPECT_EQ(packs, packs1); // no new packs loaded
  ASSERT_NE(packs1.begin(), packs1.end());
  auto pack1 = *packs1.begin();
  ASSERT_TRUE(pack1 != nullptr);

  EXPECT_EQ(pack1->GetFirstChild("dummy_child"), dummyChild);
  // but replace if requested
  packs1.clear();
  EXPECT_TRUE(rteKernel.LoadPacks(files, packs1, &testModel, true));
  EXPECT_EQ(packs1.size(), files.size());
  pack1 = *packs1.begin();
  EXPECT_EQ(pack1->GetFirstChild("dummy_child"), nullptr); // pack got loaded again => no added child

  EXPECT_EQ(packRegistry->GetLoadedPacks().size(), files.size());
  ASSERT_NE(files.begin(), files.end());
  const string& firstFile = *files.begin();
  pack = packRegistry->GetPack(firstFile);
  ASSERT_TRUE(pack != nullptr);
  EXPECT_EQ(pack->GetPackageState(), PackageState::PS_INSTALLED);
  EXPECT_TRUE(packRegistry->ErasePack(firstFile));
  EXPECT_FALSE(packRegistry->GetPack(firstFile) != nullptr);
  EXPECT_FALSE(packRegistry->ErasePack(firstFile)); // already not in collection
  packs.clear();
  EXPECT_TRUE(rteKernel.LoadPacks(files, packs, &testModel));
  EXPECT_EQ(packs.size(), packs1.size()); //only one pack is loaded
  packs.clear();
}

TEST(RteModelTest, LoadPacks) {

  RteKernelSlim rteKernel;  // here just to instantiate XMLTree parser
  list<string> latestFiles;
  EXPECT_FALSE(rteKernel.GetEffectivePdscFiles(latestFiles, true));

  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);

  EXPECT_TRUE(rteKernel.GetEffectivePdscFiles(latestFiles, true));
  EXPECT_EQ(latestFiles.size(), 8);

  list<string> files;
  rteKernel.GetEffectivePdscFiles(files, false);
  EXPECT_EQ(files.size(), 11);

  RteModel* rteModel = rteKernel.GetGlobalModel();
  ASSERT_NE(rteModel, nullptr);
  rteModel->SetUseDeviceTree(true);
  list<RtePackage*> packs;
  EXPECT_TRUE(rteKernel.LoadPacks(files, packs));
  EXPECT_TRUE(!packs.empty());
  rteModel->InsertPacks(packs);

  bool rteModelValidateResult = rteModel->Validate();
  EXPECT_TRUE(rteModelValidateResult);

  RtePackage* pack = rteModel->GetPackage("ARM::RteTest@0.1.0");
  ASSERT_TRUE(pack != nullptr);
  RtePackageMap requiredPacks;
  pack->GetRequiredPacks(requiredPacks, rteModel);
  EXPECT_EQ(requiredPacks.size(), 1);

  // do not clean requiredPacks
  pack = rteModel->GetPackage("ARM::RteTestRequired@1.0.0");
  ASSERT_TRUE(pack != nullptr);
  pack->GetRequiredPacks(requiredPacks, rteModel);
  EXPECT_EQ(requiredPacks.size(), 4);

  RteDeviceItemAggregate* da = rteModel->GetDeviceAggregate("RteTest_ARMCM3", "ARM:82");
  ASSERT_NE(da, nullptr);
  // test deprecated memory attributes: IROM and IRAM
  string summary = da->GetSummaryString();
  EXPECT_EQ(summary, "ARM Cortex-M3, 10 MHz, 128 kB RAM, 256 kB ROM");

  da = rteModel->GetDeviceAggregate("RteTest_ARMCM4", "ARM:82");
  ASSERT_NE(da, nullptr);
  // test recommended memory attributes: name and access
  summary = da->GetSummaryString();
  EXPECT_EQ(summary, "ARM Cortex-M4, 10 MHz, 128 kB RAM, 256 kB ROM");

  RteBoard* board = rteModel->FindBoard("RteTest board listing (Rev.C)");
  ASSERT_NE(board, nullptr);
  EXPECT_TRUE(board->HasMCU());
  Collection<RteItem*> algos;
  EXPECT_EQ(board->GetAlgorithms(algos).size(), 2);
  Collection<RteItem*> mems;
  EXPECT_EQ(board->GetMemories(mems).size(), 2);

  pack = board->GetPackage();
  ASSERT_TRUE(pack != nullptr);
  RtePackageInfo pi(pack);
  EXPECT_TRUE(pi.HasAttribute("description"));
  EXPECT_EQ(pi.GetDescription(), pack->GetDescription());
  EXPECT_EQ(pi.GetID(), "ARM::RteTestBoard@0.1.0");

  board = rteModel->FindBoard("RteTest NoMCU board");
  ASSERT_NE(board, nullptr);
  EXPECT_FALSE(board->HasMCU());
  algos.clear();
  EXPECT_EQ(board->GetAlgorithms(algos).size(), 0);
  mems.clear();
  EXPECT_EQ(board->GetMemories(mems).size(), 2);
  // find components
  RteComponentInstance item(nullptr);
  item.SetTag("component");
  item.SetAttributes({ {"Cclass","RteTest" },
                       {"Cgroup", "Check" },
                       {"Csub", "Missing"},
                       {"Cversion","0.9.9"},
                       {"condition","Missing"}});
  RtePackageInstanceInfo packInfo(nullptr, "ARM::RteTest@0.1.0");
  item.SetPackageAttributes(packInfo);
  list<RteComponent*> components;
  RteComponent* c = rteModel->FindComponents(item, components);
  EXPECT_EQ(components.size(), 1);
  EXPECT_TRUE(c != nullptr);
  components.clear();
  packInfo.SetPackId("ARM::RteTest");
  item.SetPackageAttributes(packInfo);
  c = rteModel->FindComponents(item, components);
  EXPECT_EQ(components.size(), 1);
  EXPECT_TRUE(c != nullptr);

  components.clear();
  packInfo.SetPackId("ARM::RteTest");
  item.SetPackageAttributes(packInfo);
  item.RemoveAttribute("Csub");
  item.RemoveAttribute("Cversion");
  item.RemoveAttribute("condition");
  c = rteModel->FindComponents(item, components);
  EXPECT_EQ(components.size(), 3);
  ASSERT_TRUE(c != nullptr);
  EXPECT_EQ(c->GetCsubName(), "Incompatible"); // first with such attributes

  components.clear();
  item.SetAttribute("Cclass", "RteTestBundle");
  item.SetAttribute("Cgroup", "G0");
  c = rteModel->FindComponents(item, components);
  EXPECT_EQ(components.size(), 1);
  ASSERT_TRUE(c != nullptr);
  EXPECT_EQ(c->GetVersionString(), "0.9.0");

  components.clear();
  item.SetAttribute("Cbundle", "BundleTwo");
  item.SetAttribute("Cgroup", "G0");
  c = rteModel->FindComponents(item, components);
  EXPECT_EQ(components.size(), 1);
  ASSERT_TRUE(c != nullptr);
  EXPECT_EQ(c->GetVersionString(), "2.0.0");

  components.clear();
  item.SetAttribute("Cbundle", "BundleNone");
  c = rteModel->FindComponents(item, components);
  EXPECT_EQ(components.size(), 0);
  EXPECT_FALSE(c != nullptr);

  components.clear();
  item.SetAttribute("Cbundle", "BundleTwo");
  packInfo.SetPackId("ARM::RteTest@1.0");
  item.SetPackageAttributes(packInfo);
  c = rteModel->FindComponents(item, components);
  EXPECT_EQ(components.size(), 0);
  EXPECT_FALSE(c != nullptr);

  // get API
  const string& apiId = "::RteTest:CORE(API)";
  RteApi* api = rteModel->GetLatestApi(apiId);
  ASSERT_TRUE(api);
  EXPECT_EQ(api->GetID(),  "::RteTest:CORE(API)@1.1.2");
  EXPECT_EQ(api->GetPackageID(), "ARM::RteTest_DFP@0.2.0");

  // make pack "dominant"
  pack = rteModel->GetPackage("ARM::RteTest_DFP@0.1.1");
  ASSERT_TRUE(pack);
  RteItem* dominateItem = new RteItem("dominate", pack);
  pack->AddChild(dominateItem);
  pack->Construct(); // refresh internal state
  api = rteModel->GetLatestApi(apiId);
  ASSERT_TRUE(api);
  EXPECT_EQ(api->GetID(),  "::RteTest:CORE(API)@1.1.1");
  EXPECT_EQ(api->GetPackageID(), "ARM::RteTest_DFP@0.1.1");
}

class RteModelPrjTest : public RteModelTestConfig
{
protected:
  struct ToolInfo {
    string Name;
    string Version;
  } m_toolInfo;

  bool HeaderContainsToolInfo(const string& fileName);
  void GenerateHeadersTest(const string& project, const string& rteFolder,
    const bool& removeExistingHeaders = false, const bool& expectHeaderUpdate = false);

};

bool RteModelPrjTest::HeaderContainsToolInfo(const string& fileName) {
  string fileBuffer, capToolName = m_toolInfo.Name;
  string expectHeaderInfo1, expectHeaderInfo2;

  // Open file and read content
  if (!RteFsUtils::ReadFile(fileName, fileBuffer)) {
    return false;
  }

  transform(capToolName.begin(), capToolName.end(), capToolName.begin(), ::toupper);
  expectHeaderInfo1 = capToolName + " generated file: DO NOT EDIT!";
  expectHeaderInfo2 = "Generated by: " + m_toolInfo.Name + " version " + m_toolInfo.Version;

  size_t pos1, pos2;
  // check if the tool info is updated in the header
  pos1 = fileBuffer.find(expectHeaderInfo1, 0);
  pos2 = fileBuffer.find(expectHeaderInfo2, pos1);
  return ((pos1 != std::string::npos && pos2 != std::string::npos)) ? true : false;
}

void RteModelPrjTest::GenerateHeadersTest(const string& project, const string& rteFolder,
  const bool& removeExistingHeaders, const bool& expectHeaderUpdate) {

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

  if (removeExistingHeaders) {
    // remove header files
    RteFsUtils::DeleteFileAutoRetry(preIncComp);
    RteFsUtils::DeleteFileAutoRetry(preIncGlob);
    RteFsUtils::DeleteFileAutoRetry(rteComp);
  }
  // load cprj test project
  RteKernelSlim rteKernel;
  RteCallback rteCallback;
  rteKernel.SetRteCallback(&rteCallback);
  rteCallback.SetRteKernel(&rteKernel);

  XmlItem attributes;
  attributes.AddAttribute("name", m_toolInfo.Name);
  attributes.AddAttribute("version", m_toolInfo.Version);
  rteKernel.SetToolInfo(attributes);

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
  EXPECT_EQ(!expectHeaderUpdate, RteFsUtils::CmpFileMem(preIncComp, preIncCompBuf));
  EXPECT_EQ(!expectHeaderUpdate, RteFsUtils::CmpFileMem(preIncGlob, preIncGlobBuf));
  EXPECT_EQ(!expectHeaderUpdate, RteFsUtils::CmpFileMem(rteComp, rteCompBuf));

  // Check if the header of the file is updated
  EXPECT_EQ(expectHeaderUpdate, HeaderContainsToolInfo(preIncComp));
  EXPECT_EQ(expectHeaderUpdate, HeaderContainsToolInfo(preIncGlob));
  EXPECT_EQ(expectHeaderUpdate, HeaderContainsToolInfo(rteComp));

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

TEST_F(RteModelPrjTest, LoadCprj) {

  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);

  RteLicenseInfoCollection licences;
  licences.AddLicenseInfo(nullptr);
  EXPECT_TRUE(licences.ToString().empty());

  loadedCprjProject->CollectLicenseInfos(licences);
  string licenseText = licences.ToString();

  string licRefFile = prjsDir + RteTestM3 + "/license_info_ref.txt";
  EXPECT_TRUE(RteFsUtils::CmpFileMem(licRefFile, licenseText));

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

  auto componentClass = activeTarget->GetComponentClass("RteTestBundle");
  ASSERT_NE(componentClass, nullptr);
  RteBundle* bundle = componentClass->GetSelectedBundle();
  ASSERT_NE(bundle, nullptr);
  EXPECT_EQ(bundle->GetCbundleName(), componentClass->GetSelectedBundleName());

  map<const RteItem*, RteDependencyResult> depResults;
  RteItem::ConditionResult res = activeTarget->GetDepsResult(depResults, activeTarget);
  EXPECT_EQ(res, RteItem::FULFILLED);
 // component is resolved to empty variant
  RteComponentAggregate* ca = activeTarget->GetComponentAggregate("ARM::RteTest:Dependency:Variant");
  ASSERT_NE(ca, nullptr);
  RteComponentInstance* ci = ca->GetComponentInstance();
  ASSERT_NE(ci, nullptr);
  RteComponent* c = ci->GetResolvedComponent(activeTarget->GetName());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c, ca->GetComponent());
  EXPECT_FALSE(c->IsDefaultVariant());
  EXPECT_TRUE(c->GetCvariantName().empty());

  string boardName = activeTarget->GetAttribute("Bname");
  EXPECT_EQ(boardName, "RteTest Test board");
  // get layers
  auto& allLayerDescriptors = rteKernel.GetGlobalModel()->GetLayerDescriptors();
  EXPECT_EQ(allLayerDescriptors.size(), 10);
  auto& filteredLayerDescriptors = activeTarget->GetFilteredModel()->GetLayerDescriptors();
  EXPECT_EQ(filteredLayerDescriptors.size(), 8);

  const string rteDir = RteUtils::ExtractFilePath(RteTestM3_cprj, true) + "RTE/";
  const string CompConfig_0_Base_Version = rteDir + "RteTest/" + "ComponentLevelConfig_0.h.base@0.0.1";
  const string CompConfig_1_Base_Version = rteDir + "RteTest/" + "ComponentLevelConfig_1.h.base@0.0.1";
  EXPECT_TRUE(RteFsUtils::Exists(CompConfig_0_Base_Version));
  EXPECT_TRUE(RteFsUtils::Exists(CompConfig_1_Base_Version));
  RteFileInstance * fi = activeCprjProject->GetFileInstance("RTE/RteTest/ComponentLevelConfig_0.h");
  ASSERT_NE(fi, nullptr);
  EXPECT_EQ(fi->GetInfoString(activeTarget->GetName()),
    "RTE/RteTest/ComponentLevelConfig_0.h@0.0.1 (up to date) from ARM::RteTest:ComponentLevel@0.0.1");
  EXPECT_EQ(fi->GetInfoString(activeTarget->GetName(), prjsDir),
    "RteTestM3/RTE/RteTest/ComponentLevelConfig_0.h@0.0.1 (up to date) from ARM::RteTest:ComponentLevel@0.0.1");

  ASSERT_NE(fi, nullptr);
  fi = activeCprjProject->GetFileInstance("RTE/RteTest/ComponentLevelConfig_1.h");
  EXPECT_EQ(fi->GetInfoString(activeTarget->GetName()),
    "RTE/RteTest/ComponentLevelConfig_1.h@0.0.1 (up to date) from ARM::RteTest:ComponentLevel@0.0.1");
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
  fi = activeCprjProject->GetFileInstance("RTE/Device/RteTest_ARMCM3/system_ARMCM3.c");
  ASSERT_NE(fi, nullptr);
  EXPECT_EQ(fi->GetInfoString(activeTarget->GetName()),
    "RTE/Device/RteTest_ARMCM3/system_ARMCM3.c@1.0.1 (update@1.2.2) from ARM::Device:Startup&RteTest Startup@2.0.3");

  RtePackageMap usedPacks;
  activeCprjProject->GetUsedPacks(usedPacks, activeTarget->GetName());
  EXPECT_EQ(usedPacks.size(), 2);

  RtePackageMap requiredPacks;
  activeCprjProject->GetRequiredPacks(requiredPacks, activeTarget->GetName());
  // requirements overlap => more than used
  EXPECT_EQ(requiredPacks.size(), 3);
}

TEST_F(RteModelPrjTest, ExtGenAndAccessSeq) {

  RteCallback callback;
  RteKernelSlim rteKernel(&callback);
  callback.SetRteKernel(&rteKernel);
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  string absPath = RteFsUtils::MakePathCanonical(RteFsUtils::AbsolutePath(RteModelTestConfig::LOCAL_REPO_DIR).generic_string());
  rteKernel.SetCmsisToolboxDir(absPath);
  rteKernel.Init();

  // load all installed packs
  list<string> files;
  rteKernel.GetEffectivePdscFiles(files, false);
  RteModel* rteModel = rteKernel.GetGlobalModel();
  ASSERT_TRUE(rteModel);
  rteModel->SetUseDeviceTree(true);
  list<RtePackage*> packs;
  EXPECT_TRUE(rteKernel.LoadPacks(files, packs));
  rteModel->InsertPacks(packs);

  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_cprj);
  ASSERT_TRUE(loadedCprjProject);

  RteTarget* activeTarget = rteKernel.GetActiveTarget();
  ASSERT_TRUE(activeTarget);

  RteComponentInstance item(nullptr);
  item.SetAttributes({ {"Cclass","RteTestGenerator" },
                       {"Cgroup", "Check Global Generator" },
                       {"Cversion","0.9.0"}});
  RtePackageInstanceInfo packInfo(nullptr, "ARM::RteTestGenerator");
  item.SetPackageAttributes(packInfo);
  list<RteComponent*> components;
  RteComponent* c = rteModel->FindComponents(item, components);
  ASSERT_TRUE(c);
  RteGenerator* gen = c->GetGenerator();
  ASSERT_TRUE(gen);

  string path = gen->GetExpandedWorkingDir(activeTarget);
  EXPECT_EQ(path, "RteModelTestProjects/RteTestM3/Target 1/RteTest_ARMCM3/");
  string cmd = gen->GetExpandedCommandLine(activeTarget);

  EXPECT_EQ(rteKernel.GetCmsisToolboxDir(), absPath);
  const string expectedCmd = absPath + "/bin/RunTestGen \"RteModelTestProjects/RteTestM3/Target 1.cbuild-gen-idx.yml\"";
  EXPECT_EQ(cmd, expectedCmd);

  // test additional expansions
  const string src = "$SolutionDir()$/$ProjectDir()$/$Bname$/";
  string res = activeTarget->ExpandAccessSequences(src);
  EXPECT_EQ(res, "RteModelTestProjects/RteTestM3/./RteTest Test board/");
  // set solution dir to  RteModelTestProjects
  rteModel->SetRootFileName("RteModelTestProjects/dummy.csolution.yml");
  res = activeTarget->ExpandAccessSequences(src);
  EXPECT_EQ(res, "RteModelTestProjects/RteTestM3/RteTest Test board/");
}

TEST_F(RteModelPrjTest, LoadCprjPacReq) {

  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3PackReq_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);
  RteTarget* activeTarget = loadedCprjProject->GetActiveTarget();
  ASSERT_NE(activeTarget, nullptr);

  RtePackageMap usedPacks;
  loadedCprjProject->GetUsedPacks(usedPacks, activeTarget->GetName());
  EXPECT_EQ(usedPacks.size(), 2);

  RtePackageMap requiredPacks;
  loadedCprjProject->GetRequiredPacks(requiredPacks, activeTarget->GetName());
  // requirements overlap and not all loaded => more than used
  EXPECT_EQ(requiredPacks.size(), 6);
}

TEST_F(RteModelPrjTest, LoadCprj_NoRTEFileCreation) {

  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3_cprj, RteUtils::EMPTY_STRING, true, false);
  ASSERT_NE(loadedCprjProject, nullptr);
  RteTarget* activeTarget = loadedCprjProject->GetActiveTarget();
  ASSERT_NE(activeTarget, nullptr);

  const string rteDir = RteUtils::ExtractFilePath(RteTestM3_cprj, true) + "RTE/";
  const string CompConfig_0_Base_Version = rteDir + "RteTest/" + "ComponentLevelConfig_0.h.base@0.0.1";
  const string CompConfig_1_Base_Version = rteDir + "RteTest/" + "ComponentLevelConfig_1.h.base@0.0.1";
  EXPECT_FALSE(RteFsUtils::Exists(CompConfig_0_Base_Version));
  EXPECT_FALSE(RteFsUtils::Exists(CompConfig_1_Base_Version));
  RteFileInstance* fi = loadedCprjProject->GetFileInstance("RTE/RteTest/ComponentLevelConfig_0.h");
  ASSERT_NE(fi, nullptr);
  EXPECT_EQ(fi->GetInfoString(activeTarget->GetName()),
    "RTE/RteTest/ComponentLevelConfig_0.h@0.0.1 (up to date) from ARM::RteTest:ComponentLevel@0.0.1");

  const string deviceDir = rteDir + "Device/RteTest_ARMCM3/";
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "ARMCM3_ac6.sct.update@1.2.0"));
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "startup_ARMCM3.c.base@2.0.3"));
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "system_ARMCM3.c.update@1.2.2"));

  fi = loadedCprjProject->GetFileInstance("RTE/Device/RteTest_ARMCM3/system_ARMCM3.c");
  ASSERT_NE(fi, nullptr);
  EXPECT_EQ(fi->GetInfoString(activeTarget->GetName()),
    "RTE/Device/RteTest_ARMCM3/system_ARMCM3.c@1.0.1 (update@1.2.2) from ARM::Device:Startup&RteTest Startup@2.0.3");
  RteFsUtils::RemoveFile(fi->GetAbsolutePath());
  EXPECT_EQ(fi->GetInfoString(activeTarget->GetName()),
    "RTE/Device/RteTest_ARMCM3/system_ARMCM3.c@1.0.1 (not exist) from ARM::Device:Startup&RteTest Startup@2.0.3");

  // additionally test support for RTE folder with spaces
  // take existing file instance
  // use its file to create path with another RTE directory
  RteFile* f = fi ? fi->GetFile(loadedCprjProject->GetActiveTargetName()) : nullptr ;
  ASSERT_NE(f, nullptr);
  const string& deviceName = loadedCprjProject->GetActiveTarget()->GetDeviceName();
  string pathName = f->GetInstancePathName(deviceName, 0 , "RTE With Spaces");
  EXPECT_EQ(pathName, "RTE With Spaces/Device/RteTest_ARMCM3/system_ARMCM3.c");
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
  const string CompConfig_0 = rteDir + "RteTest/" + "ComponentLevelConfig_0.h";
  const string CompConfig_0_Base_Version = CompConfig_0 + ".base@0.0.1";
  EXPECT_TRUE(RteFsUtils::Exists(CompConfig_0));
  EXPECT_TRUE(RteFsUtils::Exists(CompConfig_0_Base_Version));
  const string CompConfig_1 = rteDir + "RteTest/" + "ComponentLevelConfig_1.h";
  const string CompConfig_1_Base_Version = CompConfig_1 + ".base@0.0.1";
  EXPECT_TRUE(RteFsUtils::Exists(CompConfig_1));
  EXPECT_TRUE(RteFsUtils::Exists(CompConfig_1_Base_Version));
  // check if freshly copied files match their base
  // that also checks if base files expand %Instance% with correct instance
  string buf, bufBase;
  EXPECT_TRUE(RteFsUtils::ReadFile(CompConfig_0, buf));
  EXPECT_TRUE(RteFsUtils::ReadFile(CompConfig_0_Base_Version, bufBase));
  EXPECT_EQ(buf, bufBase);

  EXPECT_TRUE(RteFsUtils::ReadFile(CompConfig_1, buf));
  EXPECT_TRUE(RteFsUtils::ReadFile(CompConfig_1_Base_Version, bufBase));
  EXPECT_EQ(buf, bufBase);

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
  rteKernel.SetCmsisPackRoot(packsDir);

  XmlItem attributes;
  auto pdsc = rteKernel.GetLocalPdscFile(attributes);
  EXPECT_TRUE(pdsc.first.empty());
  EXPECT_TRUE(pdsc.second.empty());

  attributes.AddAttribute("name", "LocalPack");
  attributes.AddAttribute("vendor", "LocalVendor");
  pdsc = rteKernel.GetLocalPdscFile(attributes);

  // check returned packId
  EXPECT_EQ(pdsc.first, "LocalVendor::LocalPack@1.0.1");

  // check returned pdsc
  const string expectedPdsc =
    RteFsUtils::MakePathCanonical(RteFsUtils::AbsolutePath(localPacks + "/L/LocalVendor.LocalPack.pdsc").generic_string());
  error_code ec;
  EXPECT_TRUE(fs::equivalent(pdsc.second, expectedPdsc, ec));
}

TEST_F(RteModelPrjTest, GetInstalledPdscFile) {
  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(packsDir);

  XmlItem attributes;
  auto pdsc = rteKernel.GetInstalledPdscFile(attributes);
  EXPECT_TRUE(pdsc.first.empty());
  EXPECT_TRUE(pdsc.second.empty());

  attributes.AddAttribute("name", "RteTestRequired");
  attributes.AddAttribute("vendor", "ARM");
  pdsc = rteKernel.GetInstalledPdscFile(attributes);

  // check returned packId
  EXPECT_EQ(pdsc.first, "ARM::RteTestRequired@1.0.0");

  // check returned pdsc
  const string expectedPdsc =
    RteFsUtils::MakePathCanonical(RteFsUtils::AbsolutePath(packsDir +
      "/ARM/RteTestRequired/1.0.0/ARM.RteTestRequired.pdsc").generic_string());
  error_code ec;
  EXPECT_TRUE(fs::equivalent(pdsc.second, expectedPdsc, ec));
}

TEST_F(RteModelPrjTest, GetEffectivePdscFile) {
  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(packsDir);
  XmlItem attributes;

  // nothing has found for empty attributes
  auto pdsc = rteKernel.GetInstalledPdscFile(attributes);
  EXPECT_TRUE(pdsc.first.empty());
  EXPECT_TRUE(pdsc.second.empty());

  // local and installed equal => local
  attributes.AddAttribute("name", "RteTest");
  attributes.AddAttribute("vendor", "SomeVendor");
  pdsc = rteKernel.GetEffectivePdscFile(attributes);
  EXPECT_EQ(pdsc.first, "SomeVendor::RteTest@0.0.1");
  string expectedPdsc =
    RteFsUtils::MakePathCanonical(RteFsUtils::AbsolutePath(localPacks + "/S/SomeVendor.RteTest.pdsc").generic_string());
  error_code ec;
  EXPECT_TRUE(fs::equivalent(pdsc.second, expectedPdsc, ec));

  // local is newer
  attributes.AddAttribute("name", "RteTestRequired");
  attributes.AddAttribute("vendor", "ARM");
  pdsc = rteKernel.GetEffectivePdscFile(attributes);

  EXPECT_EQ(pdsc.first, "ARM::RteTestRequired@1.0.1-local");
  expectedPdsc =
    RteFsUtils::MakePathCanonical(RteFsUtils::AbsolutePath(localPacks + "/A/ARM.RteTestRequired.pdsc").generic_string());
  EXPECT_TRUE(fs::equivalent(pdsc.second, expectedPdsc, ec));

  // installed is newer
  attributes.AddAttribute("name", "RteTestRequiredRecursive");
  pdsc = rteKernel.GetEffectivePdscFile(attributes);
  EXPECT_EQ(pdsc.first, "ARM::RteTestRequiredRecursive@1.0.0");
  expectedPdsc =
    RteFsUtils::MakePathCanonical(RteFsUtils::AbsolutePath(packsDir +
      "/ARM/RteTestRequiredRecursive/1.0.0/ARM.RteTestRequiredRecursive.pdsc").generic_string());
  EXPECT_TRUE(fs::equivalent(pdsc.second, expectedPdsc, ec));

  // specific version
  attributes.AddAttribute("version", "1.0.0-local:1.0.0-local");
  pdsc = rteKernel.GetEffectivePdscFile(attributes);
  EXPECT_EQ(pdsc.first, "ARM::RteTestRequiredRecursive@1.0.0-local");
  expectedPdsc =
    RteFsUtils::MakePathCanonical(RteFsUtils::AbsolutePath(localPacks + "/R/ARM.RteTestRequiredRecursive.pdsc").generic_string());
  EXPECT_TRUE(fs::equivalent(pdsc.second, expectedPdsc, ec));

  // outside range
  attributes.AddAttribute("version", "2.0.0");
  pdsc = rteKernel.GetInstalledPdscFile(attributes);
  EXPECT_TRUE(pdsc.first.empty());
  EXPECT_TRUE(pdsc.second.empty());

  // unknown name
  attributes.RemoveAttribute("version");
  attributes.AddAttribute("name", "Unknown");
  pdsc = rteKernel.GetInstalledPdscFile(attributes);
  EXPECT_TRUE(pdsc.first.empty());
  EXPECT_TRUE(pdsc.second.empty());
}


TEST_F(RteModelPrjTest, GenerateHeadersTestDefault)
{
  m_toolInfo = ToolInfo{ "TestExe", "1.0.0" };
  GenerateHeadersTest(RteTestM3_cprj, "RTE");
}

TEST_F(RteModelPrjTest, GenerateHeadersTestDefault_Update_Header)
{
  m_toolInfo = ToolInfo{ "TestExe", "2.0.0" };
  GenerateHeadersTest(RteTestM3_cprj, "RTE", true, true);
}

TEST_F(RteModelPrjTest, GenerateHeadersTest_ConfigFolder)
{
  GenerateHeadersTest(RteTestM3_ConfigFolder_cprj, "CONFIG_FOLDER");
}

TEST_F(RteModelPrjTest, GenerateHeadersTest_Update_Header)
{
  m_toolInfo = ToolInfo{ "TestExe", "3.0.0" };
  GenerateHeadersTest(RteTestM3_UpdateHeader_cprj, "RTE_Update_Header", false, true);
}

TEST_F(RteModelPrjTest, RteNoComponents)
{
  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM3NoComponents_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);
  // check if neither RTE directory is created, nor RTE_Ceomponents.h
  const string& rteFolder = loadedCprjProject->GetRteFolder();
  EXPECT_EQ("RTE_NO_DIR", rteFolder);
  const string rteDir = RteUtils::ExtractFilePath(RteTestM3NoComponents_cprj, true) + rteFolder;
  const string targetFolder = "/_Target_1/";
  const string rteComp = rteDir + targetFolder + "RTE_Components.h";
  EXPECT_FALSE(RteFsUtils::Exists(rteDir));
  EXPECT_FALSE(RteFsUtils::Exists(rteComp));
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
  const string newFile = cprjFile->GetRootFileName();
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

TEST_F(RteModelPrjTest, LoadCprjM4) {

  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM4_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);

  // check if active project is set
  RteCprjProject* activeCprjProject = rteKernel.GetActiveCprjProject();
  ASSERT_NE(activeCprjProject, nullptr);
  // check if it is the loaded project
  EXPECT_EQ(activeCprjProject, loadedCprjProject);

  RteDeviceItem* device = rteKernel.GetActiveDevice();
  string deviceName = device ? device->GetName() : RteUtils::ERROR_STRING;
  string deviceVendor = device ? device->GetVendorString() : RteUtils::ERROR_STRING;
  EXPECT_EQ(deviceName, "RteTest_ARMCM4_FP");

  RteTarget* activeTarget = activeCprjProject->GetActiveTarget();
  ASSERT_NE(activeTarget, nullptr);
  map<const RteItem*, RteDependencyResult> depResults;
  RteItem::ConditionResult res = activeTarget->GetDepsResult(depResults, activeTarget);
  EXPECT_EQ(res, RteItem::FULFILLED);
 // component variant is resolved to the default one
  RteComponentAggregate* ca= activeTarget->GetComponentAggregate("ARM::RteTest:Dependency:Variant");
  ASSERT_NE(ca, nullptr);
  RteComponentInstance* ci = ca->GetComponentInstance();
  ASSERT_NE(ci, nullptr);
  RteComponent* c = ci->GetResolvedComponent(activeTarget->GetName());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c, ca->GetComponent());
  EXPECT_TRUE(c->IsDefaultVariant());
  EXPECT_EQ(c->GetCvariantName(), "Compatible");
  string boardName = activeTarget->GetAttribute("Bname");
  EXPECT_TRUE(boardName.empty());
  // get layers
  auto& allLayerDescriptors = rteKernel.GetGlobalModel()->GetLayerDescriptors();
  EXPECT_EQ(allLayerDescriptors.size(), 10);
  auto& filteredLayerDescriptors = activeTarget->GetFilteredModel()->GetLayerDescriptors();
  EXPECT_EQ(filteredLayerDescriptors.size(), 10);

  const string projDir = RteUtils::ExtractFilePath(RteTestM4_cprj, true);
  const string rteDir = projDir + "RTE/";
  const string CompConfig_0_Base_Version = rteDir + "RteTest/" + "ComponentLevelConfig_0.h.base@0.0.1";
  const string CompConfig_1_Base_Version = rteDir + "RteTest/" + "ComponentLevelConfig_1.h.base@0.0.1";
  EXPECT_TRUE(RteFsUtils::Exists(CompConfig_0_Base_Version));
  EXPECT_TRUE(RteFsUtils::Exists(CompConfig_1_Base_Version));

  error_code ec;
  const fs::perms write_mask = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;
  // check config file PLM: existence and permissions
  const string deviceDir = rteDir + "Device/RteTest_ARMCM4_FP/";
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "ARMCM4_ac6.sct.base@1.0.0"));
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "ARMCM4_ac6.sct.update@1.2.0"));

  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "system_ARMCM4.c.base@1.0.1"));
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "system_ARMCM4.c.base@1.0.2"));

  EXPECT_TRUE(RteFsUtils::Exists(deviceDir + "startup_ARMCM4.c.base@2.0.3"));
  EXPECT_EQ((fs::status(deviceDir + "startup_ARMCM4.c.base@2.0.3", ec).permissions() & write_mask), fs::perms::none);

  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "system_ARMCM4.c.update@1.2.2"));

  // test regions_h
  string regionsFile = deviceDir + "regions_RteTest_ARMCM4_FP.h";
  EXPECT_EQ(activeCprjProject->GetRegionsHeader(activeTarget->GetName(), rteDir), regionsFile);
  EXPECT_TRUE(activeTarget->GenerateRegionsHeader(rteDir));
  ASSERT_TRUE(RteFsUtils::Exists(regionsFile));

  string generatedContent;
  RteFsUtils::ReadFile(regionsFile, generatedContent);

  string referenceContent;
  string refFile = projDir + "regions_RteTest_ARMCM4_FP_ref.h";
  RteFsUtils::ReadFile(refFile, referenceContent);
  EXPECT_EQ(generatedContent, referenceContent);
}

TEST_F(RteModelPrjTest, LoadCprjM4_Board) {

  RteKernelSlim rteKernel;
  rteKernel.SetCmsisPackRoot(RteModelTestConfig::CMSIS_PACK_ROOT);
  RteCprjProject* loadedCprjProject = rteKernel.LoadCprj(RteTestM4_Board_cprj);
  ASSERT_NE(loadedCprjProject, nullptr);

  // check if active project is set
  RteCprjProject* activeCprjProject = rteKernel.GetActiveCprjProject();
  ASSERT_NE(activeCprjProject, nullptr);
  // check if it is the loaded project
  EXPECT_EQ(activeCprjProject, loadedCprjProject);

  RteDeviceItem* device = rteKernel.GetActiveDevice();
  string deviceName = device ? device->GetName() : RteUtils::ERROR_STRING;
  string deviceVendor = device ? device->GetVendorString() : RteUtils::ERROR_STRING;
  EXPECT_EQ(deviceName, "RteTest_ARMCM4_FP");

  RteTarget* activeTarget = activeCprjProject->GetActiveTarget();
  ASSERT_NE(activeTarget, nullptr);
  map<const RteItem*, RteDependencyResult> depResults;
  RteItem::ConditionResult res = activeTarget->GetDepsResult(depResults, activeTarget);
  EXPECT_EQ(res, RteItem::FULFILLED);
  string boardName = activeTarget->GetAttribute("Bname");
  EXPECT_EQ(boardName, "RteTest CM4 board");
  // get layers
  auto& allLayerDescriptors = rteKernel.GetGlobalModel()->GetLayerDescriptors();
  EXPECT_EQ(allLayerDescriptors.size(), 10);
  auto& filteredLayerDescriptors = activeTarget->GetFilteredModel()->GetLayerDescriptors();
  EXPECT_EQ(filteredLayerDescriptors.size(), 7);

  const string projDir = RteUtils::ExtractFilePath(RteTestM4_Board_cprj, true);
  const string rteDir = projDir + "RTE_BOARD/";
  const string CompConfig_0_Base_Version = rteDir + "RteTest/" + "ComponentLevelConfig_0.h.base@0.0.1";
  const string CompConfig_1_Base_Version = rteDir + "RteTest/" + "ComponentLevelConfig_1.h.base@0.0.1";
  EXPECT_TRUE(RteFsUtils::Exists(CompConfig_0_Base_Version));
  EXPECT_TRUE(RteFsUtils::Exists(CompConfig_1_Base_Version));

  // expect enforced component is resolved
  RteComponentInstance* ci = activeCprjProject->GetComponentInstance("ARM::Board:Test:Rev2@2.2.2(BoardTest2)[]");
  ASSERT_TRUE(ci != nullptr);
  RteComponent* c = ci->GetResolvedComponent(activeTarget->GetName());
  ASSERT_TRUE(c != nullptr);
  EXPECT_FALSE(activeTarget->IsComponentFiltered(c));

  error_code ec;
  const fs::perms write_mask = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;
  // check config file PLM: existence and permissions
  const string deviceDir = rteDir + "Device/RteTest_ARMCM4_FP/";
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "ARMCM4_ac6.sct.base@1.0.0"));
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "ARMCM4_ac6.sct.update@1.2.0"));

  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "system_ARMCM4.c.base@1.0.1"));
  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "system_ARMCM4.c.base@1.0.2"));

  EXPECT_TRUE(RteFsUtils::Exists(deviceDir + "startup_ARMCM4.c.base@2.0.3"));
  EXPECT_EQ((fs::status(deviceDir + "startup_ARMCM4.c.base@2.0.3", ec).permissions() & write_mask), fs::perms::none);

  EXPECT_FALSE(RteFsUtils::Exists(deviceDir + "system_ARMCM4.c.update@1.2.2"));

  // test regions_h
  string regionsFile = deviceDir + "regions_RteTest_CM4_board.h";
  EXPECT_EQ(activeCprjProject->GetRegionsHeader(activeTarget->GetName(), rteDir), regionsFile);
  EXPECT_TRUE(activeTarget->GenerateRegionsHeader(rteDir));
  EXPECT_TRUE(RteFsUtils::Exists(regionsFile));

  string generatedContent;
  RteFsUtils::ReadFile(regionsFile, generatedContent);

  string referenceContent;
  string refFile = projDir + "regions_RteTest_CM4_board_ref.h";
  RteFsUtils::ReadFile(refFile, referenceContent);
  EXPECT_EQ(generatedContent, referenceContent);

}

// end of RTEModelTest.cpp
