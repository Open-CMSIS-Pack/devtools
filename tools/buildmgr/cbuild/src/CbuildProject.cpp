/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CbuildProject.h"
#include "CbuildKernel.h"
#include "CbuildModel.h"

#include "ErrLog.h"
#include "RteCprjProject.h"
#include "RteFsUtils.h"
#include "RteGenerator.h"
#include "RteModel.h"
#include "RteValueAdjuster.h"
#include "XMLTreeSlim.h"

using namespace std;

CbuildProject::CbuildProject(RteCprjProject* project) {
  m_project = project;
}

CbuildProject::~CbuildProject() {
}

bool CbuildProject::CreateTarget(const string& targetName, const RtePackage *cprjPack,
  const string& rtePath, const string& toolchain) {
  RteItem* target = cprjPack->GetItemByTag("target");
  if (!target)
    return false;

  const auto packages = cprjPack->GetItemByTag("packages");
  if (!packages) {
    // Missing <packages> element
    LogMsg("M609", VAL("NAME", "packages"));
    return false;
  }

  // update attributes: toolchain and Dcore
  map<string, string> attributes = target->GetAttributes();
  SetToolchain(toolchain, attributes);
  if (!AddAdditionalAttributes(attributes, targetName))
    return false;

  // update target with attributes
  const RteItem* components = cprjPack->GetItemByTag("components");
  if (!UpdateTarget(components, attributes, targetName)) {
    return false;
  }

  // read gpdsc
  if (!m_project)
    return false;
  const map<string, RteGpdscInfo*>& gpdscInfos = m_project->GetGpdscInfos();
  for(auto itg = gpdscInfos.begin(); itg != gpdscInfos.end(); itg++ ) {
    RteGpdscInfo* gi = itg->second;
    RteGeneratorModel* genModel = gi->GetGeneratorModel();
    const string& gpdscFile = gi->GetAbsolutePath();
    genModel = ReadGpdscFile(gpdscFile);
    if (!genModel) {
      return false;
    }
    gi->SetGeneratorModel(genModel);
  }

  if (!gpdscInfos.empty()) {
    // update target with gpdsc model
    if (!UpdateTarget(components, attributes, targetName)) {
      return false;
    }
  }

  return true;
}

bool CbuildProject::UpdateTarget(const RteItem* components, const map<string, string> &attributes, const string& targetName) {
  if (!m_project)
    return false;

  m_project->SetActiveTarget(targetName);
  m_project->AddTarget(targetName, attributes, true, true);

  RteTarget *target = m_project->GetTarget(targetName);

  if (!target)
    return false;

  if (components) {
    set<RteComponentInstance*> unresolvedComponents;
    m_project->AddCprjComponents(components->GetChildren(), target, unresolvedComponents);
    if (!unresolvedComponents.empty()) {
      for (auto ci : unresolvedComponents) {
        const string cVendorName = ci->GetVendorName();
        const string cClassName = ci->GetCclassName();
        const string cGroupName = ci->GetCgroupName();
        const string cSubName = ci->GetCsubName();
        const string cVariantName = ci->GetCvariantName();
        string componentName;
        if (!cVendorName.empty())  componentName +=       cVendorName;
        if (!cClassName.empty())   componentName += "::" + cClassName;
        if (!cGroupName.empty())   componentName += "::" + cGroupName;
        if (!cSubName.empty())     componentName += "::" + cSubName;
        if (!cVariantName.empty()) componentName += "::" + cVariantName;
        LogMsg("M604", VAL("CMP", componentName.c_str()));
      }
      return false;
    }
  }

  m_project->CollectSettings();

  return true;
}

void CbuildProject::SetToolchain(const string& toolchain, map<string, string> &attributes) {
  if ((toolchain.compare("AC5") == 0) || (toolchain.compare("AC6") == 0)) {
    attributes["Tcompiler"] = "ARMCC";
    attributes["Toptions"] = toolchain;
  } else {
    attributes["Tcompiler"] = toolchain;
  }
}

RteDevice *CbuildProject::GetDeviceLeaf(const string& fullDeviceName, const string& deviceVendor, const string& targetName) {
  if (!m_project)
    return nullptr;

  RteDevice* device = m_project->GetModel()->GetDevice(fullDeviceName, deviceVendor);
  if (device && device->GetDeviceItemCount() == 0) // exact device is found
    return device;

  string msg;
  if (!device) {
    LogMsg("M606", VAL("DEV", fullDeviceName), VAL("VENDOR", deviceVendor));
  }
  else if (device->GetDeviceItemCount() > 0) {
    // check if we need to make a variant substitution
    const string& deviceName = device->GetName();
    device = dynamic_cast<RteDevice*>(*(device->GetDeviceItems().begin()));
    if (!device)
      return device; // should never happen as RteDevice can only have children of type RteDeviceVariant derived from RteDevice

    LogMsg("M630", VAL("DEV", deviceName), VAL("VAR", device->GetName()));
  }
  return device;
}

bool CbuildProject::AddAdditionalAttributes(map<string, string> &attributes, const string& targetName) {
  RteDeviceItem *d = GetDeviceLeaf(attributes["Dname"], attributes["Dvendor"], targetName);
  string procName = attributes["Pname"];
  RteDeviceProperty* processorProperty = NULL;
  if (!d)
    return false;

  const RteDevicePropertyMap& propMap = d->GetEffectiveProperties(procName);
  if (propMap.empty())
    return false;

  // iterate through properties:
  for (auto itpm : propMap) {
    const string& propType = itpm.first;                 // processor, feature, memory, etc.
    const list<RteDeviceProperty*>& props = itpm.second;
    for (auto p : props) {
      if (propType == "processor") {
        processorProperty = p;
        break;
      }
    }
  }
  if (processorProperty) {
    attributes["Dcore"] = processorProperty->GetAttribute("Dcore");
  }
  return true;
}

bool CbuildProject::CheckPackRequirements(const RtePackage *cprjPack, const string& rtePath, vector<CbuildPackItem>& packList) {
  const auto packages = cprjPack->GetItemByTag("packages");

  if (!packages) {
    // Missing <packages> element
    LogMsg("M609", VAL("NAME", "packages"));
    return false;
  }

  for (RteItem *it : packages->GetChildren()) {
    RteAttributes a = it->GetAttributes();

    // Skip project specific packs
    if (it->HasAttribute("path")) {
      continue;
    }

    const string& name = a.GetAttribute("name");
    const string& vendor = a.GetAttribute("vendor");
    const string& version = it->GetAttribute("version");
    string path(rtePath);
    path += '/' + vendor + '/' + name;
    string installedVersion = RteFsUtils::GetInstalledPackVersion(path, version);

    if (installedVersion.empty()) {
      string localRepoId;
      CbuildKernel::Get()->GetLocalPdscFile(a, rtePath, localRepoId);
      // Check if local repo version is accepted
      if (!localRepoId.empty()) {
        continue;
      }
      // pack is neither in pack folder nor in local repo
      // add the pack identifier to the missing packs' list
      string maxVersion = RteUtils::GetSuffix(version);
      const CbuildPackItem& pack = { vendor, name, (maxVersion.empty() ? version : maxVersion) };
      packList.push_back(pack);
    }
  }

  return true;
}

RteGeneratorModel* CbuildProject::ReadGpdscFile(const string& gpdsc)
{
  /*
  Reads gpdsc file
  */
  RteGeneratorModel* gpdscModel = 0;
  fs::path path(gpdsc);
  error_code ec;
  if (!fs::exists(path, ec)) {
    LogMsg("M204", PATH(gpdsc));
    return NULL;
  }
  XMLTreeSlim tree;
  tree.Init();
  if(!tree.AddFileName(gpdsc, true)) {
      LogMsg("M203", PATH(gpdsc));
      return NULL;
  }
  gpdscModel = new RteGeneratorModel();
  if(!gpdscModel->Construct(&tree))
  {
    LogMsg("M203", PATH(gpdsc));
    delete gpdscModel;
    return NULL;
  }
  return gpdscModel;
}
