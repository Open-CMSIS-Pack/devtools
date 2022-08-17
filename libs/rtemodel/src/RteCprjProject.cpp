/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteProject.cpp
* @brief CMSIS RTE for *.cprj files
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteCprjProject.h"

#include "RteCprjModel.h"
#include "RteCprjTarget.h"
#include "CprjFile.h"
#include "XmlFormatter.h"

using namespace std;

////////////////////////////
RteCprjProject::RteCprjProject(RteCprjModel* cprjModel) :
  RteProject(),
  m_cprjModel(cprjModel)
{
  // set project name based on filename
  SetName(RteUtils::ExtractFileBaseName(GetCprjFile()->GetPackageFileName()));
  SetProjectPath(RteUtils::ExtractFilePath(GetCprjFile()->GetPackageFileName(), true));
  SetRteFolder(GetCprjFile()->GetRteFolder());
}

RteCprjProject::~RteCprjProject()
{
  RteCprjProject::Clear();
  delete m_cprjModel;
}

void RteCprjProject::Clear()
{
  RteProject::Clear();
}

CprjFile* RteCprjProject::GetCprjFile() const
{
  RteCprjModel* cprjModel = GetCprjModel();
  if(cprjModel) {
    return cprjModel->GetCprjFile();
  }
  static CprjFile EMPTY_CPRJ_FILE(nullptr);
  return &EMPTY_CPRJ_FILE;
}

RteItem* RteCprjProject::GetCprjInfo() const
{
  CprjFile* cprjFile = GetCprjFile();
  return cprjFile ? cprjFile->GetProjectInfo() : nullptr;
}


RteItem* RteCprjProject::GetCprjComponent(const std::string& id) const
{
  auto it = m_idToCprjComponents.find(id);
  if (it != m_idToCprjComponents.end()) {
    return it->second;
  }
  return nullptr;
}

bool RteCprjProject::SetToolchain(const string& toolchain, const std::string& toolChainVersion)
{
  CprjFile* cprjFile = GetCprjFile();

  const list<RteItem*>& compilersList = cprjFile->GetCompilerRequirements();
  if(toolchain.empty() && compilersList.size() > 1) {
    GetCallback()->Err("R816", "Project supports more than one toolchain, select one to use", cprjFile->GetPackageFileName());
    return false;
  }

  for (auto compiler : compilersList) {

    const string& name = compiler->GetAttribute("name");
    const string& version = compiler->GetAttribute("version");

    if ((toolchain.empty() || toolchain == name) &&
        (toolChainVersion.empty() || version.empty() || VersionCmp::RangeCompare(toolChainVersion, version)))
    {
      m_toolchain = name;
      m_toolchainVersion = version;
      return true;
    }
  }
  m_toolchain = toolchain;

  string msg = "Toolchain not supported by project: " + toolchain;
  GetCallback()->Err("R817", msg, cprjFile->GetPackageFileName());
  return false;
}


RteTarget* RteCprjProject::CreateTarget(RteModel* filteredModel, const string& name, const map<string, string>& attributes)
{
  return new RteCprjTarget(this, filteredModel, name, attributes);
}

void RteCprjProject::Initialize()
{
  // create target based on cprj description
  CprjFile* cprjFile = GetCprjFile();
  string targetName = GetName(); // use same name as project

  RteAttributes filterAttributes;
  CprjTargetElement* cprjTarget = cprjFile->GetTargetElement();
  if(cprjTarget) {
    filterAttributes.SetAttributes(cprjTarget->GetAttributes());
  }
  FillToolchainAttributes(filterAttributes);

  AddTarget(targetName, filterAttributes.GetAttributes(), true, true);
  m_targetIDs.insert(make_pair(1, targetName)); // for now cprj project contains only one target with ID == 1
  SetActiveTarget(targetName);
  RteProject::Initialize();

  // add components
  set<RteComponentInstance*> unresolvedComponents;
  RteTarget* target= GetActiveTarget();
  AddCprjComponents(cprjFile->GetGrandChildren("components"), target, unresolvedComponents);
  if (!unresolvedComponents.empty()) {
    GetCallback()->Err("R820", "Unresolved components");
    for (auto ci : unresolvedComponents) {
      string componentName = ci->GetFullDisplayName();
      GetCallback()->Err("R821", "Unresolved component", ci->GetDisplayName());
    }
  }
}


void RteCprjProject::FillToolchainAttributes(RteAttributes &attributes) const{
  if (m_toolchain == "AC5" || m_toolchain == "AC6") {
    attributes.AddAttribute("Tcompiler","ARMCC");
    attributes.AddAttribute("Toptions", m_toolchain);
  } else {
    attributes.AddAttribute("Tcompiler", m_toolchain);
  }
}

void RteCprjProject::PropagateFilteredPackagesToTargetModel(const string& targetName)
{
  RteAttributesMap fixedPacks;
  RteAttributesMap latestPacks;
  RteModel *globalModel = GetModel();

  for (auto item : GetCprjFile()->GetPackRequirements()) {
    RteAttributes a = item->GetAttributes(); // make copy of attributes
    const string& versionRange = a.GetVersionString();
    if (versionRange.empty()) {
      string commonId = RteAttributes::GetPackageIDfromAttributes(*item, false);
      latestPacks[commonId] = a;
      continue;
    }
    RtePackage* pack = globalModel->GetPackage(a);
    if (pack) {
      a = pack->GetAttributes();
      string id = RteAttributes::GetPackageIDfromAttributes(a, true);
      fixedPacks[id] = a;
    }
  }

  RteModel *targetModel = EnsureTargetModel(targetName);
  RtePackageFilter& filter = targetModel->GetPackageFilter();
  filter.SetUseAllPacks(false);
  filter.SetSelectedPackages(fixedPacks);
  filter.SetLatestPacks(latestPacks);
  targetModel->FilterModel(globalModel, NULL);
}

RteComponentInstance* RteCprjProject::AddCprjComponent(RteItem* item, RteTarget* target) {
  RteComponentInstance* ci = RteProject::AddCprjComponent(item, target);
  m_idToCprjComponents[ci->GetID()] = item;
  return ci;
}

void RteCprjProject::ApplySelectedComponents() {
  Apply();
  CollectSettings();
  Validate();
  GenerateRteHeaders();

  // Apply selected components to CprjFile
  ApplySelectedComponentsToCprjFile();
}

void RteCprjProject::ApplySelectedComponentsToCprjFile() {
  CprjFile* cprjFile = GetCprjFile();
  if (!cprjFile)
    return;
  RteItem* cprjComponents = cprjFile->GetItemByTag("components");

  // remove component from cprj
  for (auto iter = m_idToCprjComponents.begin(); iter != m_idToCprjComponents.end();) {
    if (m_components.find(iter->first) == m_components.end()) {
      cprjComponents->RemoveItem(iter->second);
      iter = m_idToCprjComponents.erase(iter);
    } else {
      iter++;
    }
  }

  // add component to cprj
  for (auto [id, ci] : m_components) {
    if (ci->IsApi())
      continue;
    RteItem* item = nullptr;
    auto it = m_idToCprjComponents.find(id);
    if (it != m_idToCprjComponents.end()) {
      item = it->second;
    }
    if (item == nullptr) {                                    // component not existent in cprj
      item = new RteItem(cprjComponents);
      item->SetTag("component");
      item->AddAttributes(ci->GetAttributes(), true);         // take over attributes
      if (ci->GetVersionMatchMode(GetActiveTargetName()) != VersionCmp::MatchMode::FIXED_VERSION) {
        item->RemoveAttribute("Cversion");                    // specify version only if fixed
      }
      item->RemoveAttribute("condition");                     // remove generally attribute "condition"
      // TODO: add build flags to component
      ApplyComponentFilesToCprjFile(ci, item);                // add files to component
      cprjComponents->AddItem(item);                          // add component to cprj file
      m_idToCprjComponents[id] = item;
    }
    if (ci->HasMaxInstances()) {
      RteInstanceTargetInfo* info = ci->GetTargetInfo(GetActiveTargetName());
      const string& count = info->GetAttribute("instances");
      if (!count.empty())
        item->AddAttribute("instances", count);
      else
        item->AddAttribute("instances", "1");
    }
  }

  cprjComponents->SortChildren(&RteItem::CompareComponents);
}

void RteCprjProject::ApplyComponentFilesToCprjFile(RteComponentInstance* ci, RteItem* cprjComponent) {
  map<string, RteFileInstance*> files;
  GetFileInstances(ci, GetActiveTargetName(), files);
  for (auto [id, fi] : files) {
    if (fi->GetAttribute("attr") == "config") {
      RteItem* item = new RteItem(cprjComponent);
      item->SetTag("file");
      item->SetAttributes(fi->GetAttributes());
      item->RemoveAttribute("condition");
      cprjComponent->AddItem(item);
    }
  }
}
// End of RteProject.cpp
