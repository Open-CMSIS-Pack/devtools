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

#include "RteCprjTarget.h"
#include "CprjFile.h"
#include "XmlFormatter.h"

using namespace std;

////////////////////////////
RteCprjProject::RteCprjProject(CprjFile* cprjFile) :
  RteProject(),
  m_cprjFile(cprjFile)
{
  // set project name based on filename
  SetName(RteUtils::ExtractFileBaseName(GetCprjFile()->GetRootFileName()));
  SetProjectPath(GetCprjFile()->GetRootFilePath(true));
  SetRteFolder(GetCprjFile()->GetRteFolder());
}

RteCprjProject::~RteCprjProject()
{
  delete m_cprjFile;
}

CprjFile* RteCprjProject::GetCprjFile() const
{
  if (m_cprjFile)
    return m_cprjFile;
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

  auto& compilersList = cprjFile->GetCompilerRequirements();
  if(toolchain.empty() && compilersList.size() > 1) {
    GetCallback()->Err("R816", "Project supports more than one toolchain, select one to use", cprjFile->GetRootFileName());
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
  GetCallback()->Err("R817", msg, cprjFile->GetRootFileName());
  return false;
}


RteTarget* RteCprjProject::CreateTarget(RteModel* filteredModel, const string& name, const map<string, string>& attributes)
{
  RteTarget* target = new RteCprjTarget(this, filteredModel, name, attributes);
  CreateBoardInfo(target, GetCprjFile()->GetTargetElement());
  return target;
}

void RteCprjProject::Initialize()
{
  // create target based on cprj description
  CprjFile* cprjFile = GetCprjFile();
  const string& cprjName = GetName();
  size_t lastDot = cprjName.find_last_of('.');
  size_t lastPlus = cprjName.find_last_of('+');
  const string& targetName = (lastDot != string::npos) && ((lastDot+1) < cprjName.length()) ?
    cprjName.substr(lastDot+1) : (lastPlus != string::npos) && ((lastPlus+1) < cprjName.length()) ?
    cprjName.substr(lastPlus+1) : "Target 1";

  XmlItem filterAttributes;
  CprjTargetElement* cprjTarget = cprjFile->GetTargetElement();
  if(cprjTarget) {
    filterAttributes.SetAttributes(cprjTarget->GetAttributes());
  }
  FillToolchainAttributes(filterAttributes);

  AddTarget(targetName, filterAttributes.GetAttributes(), true, true);
  m_targetIDs.insert(make_pair(1, targetName)); // for now cprj project contains only one target with ID == 1
  SetActiveTarget(targetName);

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

void RteCprjProject::GetRequiredPacks(RtePackageMap& packs, const std::string& targetName) const
{
  RteTarget* t = GetTarget(targetName);
  if (!t)
    return;
  CprjFile* cprjFile = GetCprjFile();

  RtePackage::ResolveRequiredPacks(nullptr, cprjFile->GetPackRequirements(), packs, t->GetFilteredModel());
  RteProject::GetRequiredPacks(packs, targetName);
}


void RteCprjProject::FillToolchainAttributes(XmlItem &attributes) const{
  if (m_toolchain == "AC5" || m_toolchain == "AC6") {
    attributes.AddAttribute("Tcompiler","ARMCC");
    attributes.AddAttribute("Toptions", m_toolchain);
  } else {
    attributes.AddAttribute("Tcompiler", m_toolchain);
  }
}

void RteCprjProject::PropagateFilteredPackagesToTargetModel(const string& targetName)
{
  RteModel* globalModel = GetModel();
  set<string> fixedPacks;
  set<string> latestPacks;
  for (auto item : GetCprjFile()->GetPackRequirements()) {
    const string& versionRange = item->GetVersionString();
    if (versionRange.empty()) {
      string commonId = RtePackage::GetPackageIDfromAttributes(*item, false);
      latestPacks.insert(commonId);
      continue;
    }
    RtePackage* pack = globalModel->GetPackage(*item);
    if (pack) {
      fixedPacks.insert(pack->GetID());
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
  if (!cprjComponents) {
    cprjComponents = new RteComponentContainer(cprjFile);
    cprjComponents->SetTag("components");
    cprjFile->AddItem(cprjComponents);
  }

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
      if (ci->GetVersionMatchMode(GetActiveTargetName()) > VersionCmp::MatchMode::FIXED_VERSION) {
        item->RemoveAttribute("Cversion");                   // specify version only if fixed
        item->RemoveAttribute("condition");                  // remove generally attribute "condition"
      }
      // TODO: add build flags to component
      ApplyComponentFilesToCprjFile(ci, item);                // add files to component
      cprjComponents->AddItem(item);                          // add component to cprj file
      m_idToCprjComponents[id] = item;
    }
    if (ci->HasMaxInstances()) {
      RteInstanceTargetInfo* info = ci->GetTargetInfo(GetActiveTargetName());
      const string& count = info ? info->GetAttribute("instances") : RteUtils::EMPTY_STRING;
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
  GetFileInstancesForComponent(ci, GetActiveTargetName(), files);
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
