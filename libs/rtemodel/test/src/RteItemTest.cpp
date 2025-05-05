/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "RteModelTestConfig.h"

#include "RteItem.h"
#include "RteComponent.h"
#include "RteCondition.h"
#include "RteDevice.h"
#include "RteFile.h"
#include "RtePackage.h"
#include "RteFsUtils.h"
#include <map>
using namespace std;


TEST(RteItemTest, GetComponentID_all_attributes) {
  const map<string, string> attributes = {
    {"Cvendor" , "Vendor"  },
    {"Cclass"  , "Class"   },
    {"Cbundle" , "Bundle"  },
    {"Cgroup"  , "Group"   },
    {"Csub"    , "Sub"     },
    {"Cvariant", "Variant" },
    {"Cversion", "9.9.9"   },
    {"Capiversion", "1.1.1"   }
  };
  RteItem item(attributes);
  item.SetTag("require");

  EXPECT_EQ("9.9.9", item.GetSemVer());

  EXPECT_EQ("Class:Group(API)@1.1.1", item.GetApiID(true));
  EXPECT_EQ("Class:Group(API)", item.GetApiID(false));

  EXPECT_EQ("Vendor::Class&Bundle:Group:Sub&Variant@9.9.9", item.GetComponentID(true));
  EXPECT_EQ("Vendor::Class&Bundle:Group:Sub&Variant", item.GetComponentID(false));

  EXPECT_EQ("Class&Bundle:Group:Sub&Variant", item.GetPartialComponentID(true));
  EXPECT_EQ("Class:Group:Sub&Variant", item.GetPartialComponentID(false));

  EXPECT_EQ("Vendor::Class&Bundle:Group:Sub", item.GetComponentAggregateID());

  EXPECT_EQ("Vendor::Class&Bundle", item.GetBundleShortID());
  EXPECT_EQ("Vendor::Class&Bundle", item.GetBundleID(false));
  EXPECT_EQ("Vendor::Class&Bundle@9.9.9", item.GetBundleID(true));

  EXPECT_EQ("Class:Group:Sub", item.GetTaxonomyDescriptionID());

  EXPECT_EQ("require Vendor::Class&Bundle:Group:Sub&Variant@9.9.9", item.GetDependencyExpressionID());
}

TEST(RteItemTest, GetComponentID_reduced_attributes) {
  const map<string, string> attributes = {
    {"Cvendor" , "Vendor"  },
    {"Cclass"  , "Class"   },
    {"Cgroup"  , "Group"   }
  };
  RteItem item(attributes);
  item.SetTag("accept");

  EXPECT_EQ("", item.GetSemVer());
  EXPECT_EQ("accept Vendor::Class:Group", item.GetDependencyExpressionID());

  EXPECT_EQ("Vendor::Class:Group", item.GetComponentID(true));
  EXPECT_EQ("Class:Group", item.GetPartialComponentID(true));

  EXPECT_TRUE(item.GetBundleID(true).empty());
  EXPECT_TRUE(item.GetBundleShortID().empty());

  EXPECT_EQ("Class:Group", item.GetTaxonomyDescriptionID());
}

TEST(RteItemTest, ComponentAttributesFromId) {

  string id = "Vendor::Class&Bundle:Group:Sub&Variant@9.9.9";
  RteItem item("component", nullptr);
  item.SetAttributesFomComponentId(id);
  EXPECT_EQ(id, item.GetComponentID(true));

  id = "Class&Bundle:Group:Sub&Variant@9.9.9";
  item.SetAttributesFomComponentId(id);
  EXPECT_EQ(id, item.GetComponentID(true));

  id = "Vendor::Class:Group&Variant";
  item.SetAttributesFomComponentId(id);
  EXPECT_EQ(id, item.GetComponentID(true));

  id = "Class:Group:Sub&Variant";
  item.SetAttributesFomComponentId(id);
  EXPECT_EQ(id, item.GetComponentID(true));

  id = "Class:Group:&Variant";
  item.SetAttributesFomComponentId(id);
  EXPECT_EQ("Class:Group&Variant", item.GetComponentID(true));
}

TEST(RteItemTest, SemVer) {
  RteItem item;
  EXPECT_EQ("", item.GetSemVer());
  EXPECT_EQ("0.0.0", item.GetSemVer(true));
  item.SetAttribute("version", "1.0-b+m");
  EXPECT_EQ("1.0.0-b", item.GetSemVer());
}

TEST(RteItemTest, PackageID) {

  RteItem packInfo;
  packInfo.AddAttribute("name", "Name");
  packInfo.AddAttribute("vendor", "Vendor");
  packInfo.AddAttribute("version", "1.2.3-alpha+build");

  string id = RtePackage::GetPackageIDfromAttributes(packInfo, true);
  EXPECT_EQ(id, "Vendor::Name@1.2.3-alpha");

  string commonId = RtePackage::CommonIdFromId(id);
  EXPECT_EQ(commonId,"Vendor::Name");
  EXPECT_EQ(commonId, RtePackage::CommonIdFromId(commonId));
  EXPECT_EQ(commonId, RtePackage::GetPackageIDfromAttributes(packInfo, false));

  EXPECT_EQ(RtePackage::VendorFromId(id), "Vendor");
  EXPECT_EQ(RtePackage::VendorFromId(commonId), "Vendor");

  EXPECT_EQ(RtePackage::NameFromId(id), "Name");
  EXPECT_EQ(RtePackage::NameFromId(commonId), "Name");

  EXPECT_EQ(RtePackage::VersionFromId("Vendor::Name@1.2.3-alpha+build"), "1.2.3-alpha");
  EXPECT_EQ(RtePackage::VersionFromId(id), "1.2.3-alpha");
  EXPECT_TRUE(RtePackage::VersionFromId(commonId).empty());

  EXPECT_EQ(RtePackage::ReleaseVersionFromId(id), "1.2.3");
  EXPECT_EQ(RtePackage::ReleaseIdFromId(id), "Vendor::Name@1.2.3");

  EXPECT_EQ(RtePackage::PackIdFromPath("Vendor.Name.1.2.3-alpha.pdsc"), id);
  EXPECT_EQ(RtePackage::PackIdFromPath("Vendor.Name.pdsc"), commonId);
  EXPECT_EQ(RtePackage::PackIdFromPath("Vendor/Name/1.2.3-alpha/Vendor.Name.pdsc"), id);
  EXPECT_EQ(RtePackage::PackIdFromPath(".Web/Vendor.Name.pdsc"), commonId);

  RtePackage pack(nullptr, packInfo.GetAttributes());
  pack.AddAttribute("url", "https://www.keil.com/pack/");
  EXPECT_EQ(RtePackage::GetPackageFileNameFromAttributes(pack, true, ".pack"), "Vendor.Name.1.2.3-alpha.pack");
  EXPECT_EQ(RtePackage::GetPackageFileNameFromAttributes(pack, false, ".pdsc"), "Vendor.Name.pdsc");
  EXPECT_EQ(pack.GetPackagePath(), "Vendor/Name/1.2.3-alpha/");
  EXPECT_EQ(pack.GetPackagePath(false), "Vendor/Name/");
  EXPECT_EQ(pack.GetDownloadUrl(false, ".pack"), "https://www.keil.com/pack/Vendor.Name.pack");
}

TEST(RteItemTest, GetYamlDeviceAttribute) {
  const map<string, string> attributes = {
    {"Dfpu"    , "DP_FPU" },
    {"Dendian" , "Little-endian" },
    {"Dsecure" , "TZ-disabled"},
    {"Dcore" , "Cortex-M7"},
    {"Ddsp"    , ""   }};

  RteItem item(attributes);
  EXPECT_EQ(item.GetYamlDeviceAttribute("Dfpu"), "dp");
  EXPECT_EQ(item.GetYamlDeviceAttribute("Dendian"), "little");
  EXPECT_EQ(item.GetYamlDeviceAttribute("Dsecure"), "off");
  EXPECT_EQ(item.GetYamlDeviceAttribute("Dcore"), "Cortex-M7");
  EXPECT_EQ(item.GetYamlDeviceAttribute("Ddsp", "off"), "off");
  EXPECT_EQ(item.GetYamlDeviceAttribute("Dmve", "off"), "off");

  EXPECT_TRUE(item.GetYamlDeviceAttribute("Ddsp").empty());
  EXPECT_TRUE(item.GetYamlDeviceAttribute("Dmve").empty());
  EXPECT_TRUE(item.GetYamlDeviceAttribute("unknown").empty());
}

TEST(RteItemTest, GetHierarchicalGroupName) {
  unique_ptr<RteFileContainer> g0(new RteFileContainer(nullptr));
  g0->AddAttribute("group", "G0");
  RteFileContainer* g1 = new RteFileContainer(g0.get());
  g1->AddAttribute("name", "G1");
  g0->AddChild(g1);
  RteFileContainer* g2 = new RteFileContainer(g1);   // no name
  g1->AddChild(g2);
  RteFileContainer* g3 = new RteFileContainer(g2);
  g3->AddAttribute("name", "G3");
  g2->AddChild(g3);

  RteFileContainer* g4 = new RteFileContainer(g3);
  g3->AddChild(g4);

  EXPECT_EQ(g4->GetHierarchicalGroupName(), "G0:G1:G3");
}

TEST(RteItemTest, GetInstancePathName) {
  RteItem packInfo;
  packInfo.AddAttribute("name", "Name");
  packInfo.AddAttribute("vendor", "Vendor");
  packInfo.AddAttribute("version", "1.2.3");

  RtePackage pack(nullptr, packInfo.GetAttributes());
  string packFileName = "TestCmsisPackRoot/" + pack.GetPackagePath() + RtePackage::GetPackageFileNameFromAttributes(pack, false, ".pdsc");
  pack.SetRootFileName(packFileName);
  EXPECT_EQ(pack.GetPackageFileName(), packFileName);
  string cmsisPackRoot = RteFsUtils::MakePathCanonical(pack.GetAbsolutePackagePath());

  RteItem* rteItem = new RteItem("test", &pack);
  rteItem->SetAttribute("name", "MyDir/MyFile.ext");

  auto instanceFile = rteItem->GetInstancePathName("MyDevice", 0, "RTEdir");
  EXPECT_EQ(instanceFile, cmsisPackRoot + "MyDir/MyFile.ext");
  rteItem->SetAttribute("attr", "config");
  instanceFile = rteItem->GetInstancePathName("MyDevice", 1, "RTEdir");
  EXPECT_EQ(instanceFile, "RTEdir/MyFile.ext");

  // add items directly to pack, for our tests it does not matter
  RteComponent* c = new RteComponent(&pack);
  c->SetAttribute("Cclass", "Device");
  c->SetAttribute("Cgroup", "Startup");
  RteItem* fileItem = c->CreateChild("files")->CreateChild("file", "./MyDir/MyFile.c");
  instanceFile = fileItem->GetInstancePathName("MyDevice", 0, "RTEdir");
  EXPECT_EQ(instanceFile, cmsisPackRoot + "MyDir/MyFile.c");

  // test config file
  fileItem->SetAttribute("attr", "config");
  instanceFile = fileItem->GetInstancePathName("MyDevice", 0, "RTEdir");
  EXPECT_EQ(instanceFile, "RTEdir/Device/MyDevice/MyFile.c");

  // check config file with instance index
  c->SetAttribute("maxInstances", 2);
  instanceFile = fileItem->GetInstancePathName("MyDevice", 0, "RTEdir");
  EXPECT_EQ(instanceFile, "RTEdir/Device/MyDevice/MyFile_0.c");

  RteDevice* device = new RteDevice(&pack);
  device->SetAttribute("Dname", "MyDevice");
  RteItem* debugVars = device->CreateChild("debugvars");
  debugVars->SetAttribute("configfile", "MyDir/MyConfig.dbgconf");
  instanceFile = debugVars->GetInstancePathName("MyDevice", 0, "RTEdir");
  EXPECT_EQ(instanceFile, "RTEdir/Device/MyDevice/MyConfig.dbgconf");
}

// end of RteItemTest.cpp
