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


bool RteCprjProject::SetToolchain(const string& toolchain)
{
  CprjFile* cprjFile = GetCprjFile();

  const list<RteItem*>& compilersList = cprjFile->GetCompilerRequirements();
  if(toolchain.empty() && compilersList.size() > 1) {
    GetCallback()->Err("R816", "Project supports more than one toolchain, select one to use", cprjFile->GetPackageFileName());
    return false;
  }

  for (auto compiler : compilersList) {
    const string& name = compiler->GetAttribute("name");
    if (toolchain.empty() || toolchain == name) {
      m_toolchain = name;
      m_toolchainVersion = compiler->GetAttribute("version");
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
    RteAttributes a = item->GetAttributes(); // make copy of atributes
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

// End of RteProject.cpp
