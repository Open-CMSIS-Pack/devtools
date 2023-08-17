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

TEST(RteModelTest, LoadPacks) {

  RteKernelSlim rteKernel;  // here just to instantiate XMLTree parser
  list<string> files;
  RteFsUtils::GetPackageDescriptionFiles(files, RteModelTestConfig::CMSIS_PACK_ROOT, 3);
  EXPECT_TRUE(files.size() > 0);

  RteModel* rteModel = rteKernel.GetGlobalModel();
  ASSERT_NE(rteModel, nullptr);
  rteModel->SetUseDeviceTree(true);
  list<RtePackage*> packs;
  EXPECT_TRUE(rteKernel.LoadPacks(files, packs));
  EXPECT_TRUE(!packs.empty());
  rteModel->InsertPacks(packs);

  bool rteModelValidateResult = rteModel->Validate();
  EXPECT_TRUE(rteModelValidateResult);

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

  RtePackage* pack = board->GetPackage();
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
}

class RteModelPrjTest : public RteModelTestConfig
{
protected:
  struct ToolInfo {
    string Name;
    string Version;
  } m_toolInfo;

  bool HeaderContainsToolInfo(const string& fileName);
  string UpdateLocalIndex();
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

string RteModelPrjTest::UpdateLocalIndex() {
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
  EXPECT_EQ(allLayerDescriptors.size(), 8);
  auto& filteredLayerDescriptors = activeTarget->GetFilteredModel()->GetLayerDescriptors();
  EXPECT_EQ(filteredLayerDescriptors.size(), 6);

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
  EXPECT_EQ(allLayerDescriptors.size(), 8);
  auto& filteredLayerDescriptors = activeTarget->GetFilteredModel()->GetLayerDescriptors();
  EXPECT_EQ(filteredLayerDescriptors.size(), 8);

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
  EXPECT_EQ(allLayerDescriptors.size(), 8);
  auto& filteredLayerDescriptors = activeTarget->GetFilteredModel()->GetLayerDescriptors();
  EXPECT_EQ(filteredLayerDescriptors.size(), 5);

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
