/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteTarget.cpp
* @brief CMSIS RTE Data Model filtering
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteTarget.h"

#include "RteKernel.h"
#include "RteProject.h"
#include "RteInstance.h"
#include "RteModel.h"
#include "RteFsUtils.h"
#include "RteConstants.h"

#include <fstream>
#include <sstream>

using namespace std;

static const string szDevHdr = "\n"   \
"/*\n"                                 \
" * Define the Device Header File: \n" \
" */\n"                                \
"#define CMSIS_device_header "; // appends device header

static const string szDefaultRteCh = "\n"                   \
"/*\n"  \
" * Auto generated Run-Time-Environment Configuration File\n" \
" *      *** Do not modify ! ***\n"  \
" *\n";


static map<string, RteFileInfo> EMPTY_STRING_TO_INSTANCE_MAP;

RteFileInfo::RteFileInfo(RteFile::Category cat, RteComponentInstance* ci, RteFileInstance* fi) :
  m_cat(cat), m_ci(ci), m_fi(fi)
{
};

int RteFileInfo::HasNewVersion(const string& targetName) const
{
  if (m_fi)
    return m_fi->HasNewVersion(targetName);
  return 0;
}

int RteFileInfo::HasNewVersion() const
{
  if (m_fi)
    return m_fi->HasNewVersion();
  return 0;
}


bool RteFileInfo::IsConfig() const
{
  return m_fi && m_fi->IsConfig();
}


RteTarget::RteTarget(RteItem* parent, RteModel* filteredModel, const string& name, const map<string, string>& attributes) :
  RteItem(parent),
  m_filteredModel(filteredModel),
  m_bTargetSupported(false), // by default not supported
  m_effectiveDevicePackage(0),
  m_deviceStartupComponent(0),
  m_device(0),
  m_deviceEnvironment(0),
  m_bDestroy(false)
{
  m_ID = name;
  SetAttributes(attributes);
  m_classes = new RteComponentClassContainer(this);
  m_filterContext = new RteConditionContext(this);
  m_dependencySolver = new RteDependencySolver(this);
}

RteTarget::~RteTarget()
{
  m_bDestroy = true;
  RteTarget::Clear();
  m_filteredModel = NULL;
  delete m_classes;
  delete m_filterContext;
  delete m_dependencySolver;
}


void RteTarget::Clear()
{
  RteItem::Clear();
  m_selectedAggregates.clear();
  m_gpdscFileNames.clear();
  ClearFilteredComponents();
  ClearCollections();
  ClearMissingPacks();
  m_filteredModel->Clear();
  m_filterContext->Clear();
  m_dependencySolver->Clear();
  m_bTargetSupported = false;
  m_effectiveDevicePackage = NULL;
  m_device = 0;

}

string RteTarget::ExpandString(const string& str, bool bUseAccessSequences, RteItem* context) const
{
  if(bUseAccessSequences && context == this) {
    return ExpandAccessSequences(str);
  }
  return RteItem::ExpandString(str, bUseAccessSequences, context);
}

std::string RteTarget::ExpandAccessSequences(const std::string& src) const
{
  XmlItem attributes;
  // device and board
  attributes.AddAttribute(RteConstants::AS_DNAME, GetAttribute(RteConstants::AS_DNAME));
  attributes.AddAttribute(RteConstants::AS_BNAME, GetAttribute(RteConstants::AS_BNAME));
  attributes.AddAttribute(RteConstants::AS_PNAME, GetAttribute(RteConstants::AS_PNAME));
  // compiler
  const string& compiler = GetAttribute("Tcompiler");
  attributes.AddAttribute(RteConstants::AS_COMPILER, (compiler == "ARMCC") ? "AC6" : compiler);
  // target name as target type
  attributes.AddAttribute(RteConstants::AS_TARGET_TYPE, GetName());
  attributes.AddAttribute(RteConstants::AS_BUILD_TYPE, RteUtils::EMPTY_STRING);

  // project and solution
  RteProject* project = GetProject();
  string projectName = project->GetName();
  attributes.AddAttribute(RteConstants::AS_PROJECT, projectName);
  string projectDir = RteUtils::RemoveTrailingBackslash(project->GetProjectPath());

  RteModel* globalModel = GetModel();
  string solutionDir = globalModel->GetRootFilePath(false); // solution filename
  if(solutionDir.empty()) {
    solutionDir = projectDir;
    projectDir = ".";
  } else {
    projectDir = RteFsUtils::RelativePath(projectDir, solutionDir);
  }

  attributes.AddAttribute(RteConstants::AS_PROJECT_DIR, projectDir);
  attributes.AddAttribute(RteConstants::AS_PROJECT_DIR_BR, projectDir);
  attributes.AddAttribute(RteConstants::AS_SOLUTION_DIR, solutionDir);
  attributes.AddAttribute(RteConstants::AS_SOLUTION_DIR_BR, solutionDir);

  return RteUtils::ExpandAccessSequences(src, attributes.GetAttributes());
}

void RteTarget::ClearMissingPacks()
{
  t_missingPackIds.clear();
}

RteBoard* RteTarget::FindBoard(const string& displayName) const
{
  if (m_filteredModel) {
    return m_filteredModel->FindCompatibleBoard(displayName, GetDevice(), true);
  }
  return nullptr;
}

void RteTarget::GetBoards(vector<RteBoard*>& boards) const
{
  if (m_filteredModel) {
    m_filteredModel->GetCompatibleBoards(boards, GetDevice());
  }
}

RteBoardInfo* RteTarget::GetBoardInfo() const {
  RteProject* project = GetProject();
  return project ? project->GetTargetBoardInfo(GetName()) : nullptr;
}

RteBoard* RteTarget::GetBoard() const {
  RteBoardInfo* bi = GetBoardInfo();
  return bi ? bi->GetBoard() : nullptr;
}

RtePackage* RteTarget::GetBoardPackage() const {
  RteBoardInfo* bi = GetBoardInfo();
  return bi ? bi->GetPackage() : nullptr;
}

void RteTarget::SetBoard(RteBoard* board) {
  RteProject* project = GetProject();
  if (project)
    project->SetBoardInfo(GetName(), board);

  AddBoadProperties(GetDevice(), GetProcessorName());
}



bool RteTarget::IsComponentFiltered(RteComponent* c) const {
  if (!c || !IsTargetSupported())
    return false;
  if (c->IsApi())
    return true;

  for (auto it = m_filteredComponents.begin(); it != m_filteredComponents.end(); ++it) {
    if (it->second == c)
      return true;
  }
  return false;
}

RteItem::ConditionResult RteTarget::GetComponents(const map<string, string>& componentAttributes, set<RteComponent*>& components) const
{
  RteItem::ConditionResult result = RteItem::MISSING;
  for (auto it = m_filteredComponents.begin(); it != m_filteredComponents.end(); ++it) {
    RteComponent* c = it->second;
    if (c->MatchComponentAttributes(componentAttributes)) {
      components.insert(c);
      if (IsComponentSelected(c)) {
        result = RteItem::FULFILLED;
      } else if (result < RteItem::SELECTABLE) {
        result = RteItem::SELECTABLE;
      }
    }
  }
  return result;
}


RteItem::ConditionResult RteTarget::GetComponentAggregates(const XmlItem& componentAttributes, set<RteComponentAggregate*>& aggregates) const
{
  return m_classes->GetComponentAggregates(componentAttributes, aggregates);
}


int RteTarget::IsSelected() const
{
  return m_classes->IsSelected();
}


int RteTarget::IsSelected(RteComponent* c) const
{
  if (!c)
    return 0;
  if (c->IsApi())
    return IsApiSelected(dynamic_cast<RteApi*>(c));
  return IsComponentSelected(c);
}


int RteTarget::IsComponentSelected(RteComponent* c) const
{
  RteComponentAggregate* a = GetComponentAggregate(c);
  if (a && a->GetComponent() == c)
    return a->IsSelected();
  return 0;
}

int RteTarget::IsApiSelected(RteApi* a) const // means implicitly selected via components
{
  RteComponentGroup* g = GetComponentGroup(a);
  if (g && g->IsSelected())
    return 1;
  return 0;
}

bool RteTarget::SelectComponent(RteComponentAggregate* a, int count, bool bUpdateDependencies, bool bUpdateBundle)
{
  if (!a)
    return false;
  int maxInst = a->GetMaxInstances();
  if (count > maxInst)
    count = maxInst;

  if (bUpdateBundle) {
    const string& bundleName = a->GetCbundleName();
    const string& className = a->GetCclassName();
    RteComponentClass* cClass = GetComponentClass(className);
    if (cClass && cClass->HasBundleName(bundleName)) {
      cClass->SetSelectedBundleName(bundleName, true); // do not update selection
    }
  }
  a->SetSelected(count);
  UpdateSelectedAggregates(a, count);

  if (bUpdateDependencies)
    EvaluateComponentDependencies();
  return true;
}

bool RteTarget::SelectComponent(RteComponent* c, int count, bool bUpdateDependencies, bool bUpdateBundle)
{
  if (!c || c->IsApi())
    return false;
  int maxInst = c->GetMaxInstances();
  if (count > maxInst)
    count = maxInst;

  RteComponentAggregate* a = GetComponentAggregate(c);
  if (!a)
    return false;
  int nSelected = a->IsSelected();
  RteComponent* ca = a->GetComponent(); // returns active component
  if (nSelected == count && ca == c)
    return false; // no change

  a->SetSelectedVariant(c->GetCvariantName());
  a->SetSelectedVersion(c->GetVersionString());

  return SelectComponent(a, count, bUpdateDependencies, bUpdateBundle);
}

void RteTarget::UpdateSelectedAggregates(RteComponentAggregate* a, int count) {
  if (!a)
    return;
  if (count == 0) {
    auto it = m_selectedAggregates.find(a);
    if (it != m_selectedAggregates.end()) {
      m_selectedAggregates.erase(it);
    }
  } else {
    m_selectedAggregates[a] = count;
  }
}


int RteTarget::IsComponentUsed(RteComponent* c) const
{
  RteComponentInstance* ci = GetUsedComponentInstance(c);
  if (ci)
    return ci->GetInstanceCount(GetName());
  return 0;
}

RteComponentInstance* RteTarget::GetUsedComponentInstance(RteComponent* c) const
{
  RteComponentAggregate* aggr = GetComponentAggregate(c);
  if (aggr && aggr->GetComponent() == c) {
    return aggr->GetComponentInstance();
  }
  return NULL;
}

RteComponentInstance* RteTarget::GetComponentInstanceForFile(const string& filePath) const
{
  auto it = m_fileToComponentInstanceMap.find(filePath);
  if (it != m_fileToComponentInstanceMap.end())
    return it->second;
  return NULL;
}

void RteTarget::AddComponentInstanceForFile(const string& filePath, RteComponentInstance* ci) {
  m_fileToComponentInstanceMap[filePath] = ci;
}


RteItem::ConditionResult RteTarget::GetDepsResult(map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const
{
  if (target != this) {
    return RteItem::R_ERROR; // should not happen, otherwise it's a bug
  }

  RteItem::ConditionResult apiResult = FULFILLED;
  // iterate over APIs for conflicts
  for (auto ita = m_filteredApis.begin(); ita != m_filteredApis.end(); ++ita) {
    RteApi* api = ita->second;
    if (!api)
      continue;
    set<RteComponent*> components;
    RteItem::ConditionResult r = GetComponentsForApi(api, components, true);
    if (r == CONFLICT) {
      apiResult = r;
      RteDependencyResult depRes(api, r);
      for (RteComponent* c : components) {
        if (c && IsComponentFiltered(c)) {
          RteComponentAggregate* a = GetComponentAggregate(c);
          if (a)
            depRes.AddComponentAggregate(a);
        }
      }
      results[api] = depRes;
    }
  }
  RteItem::ConditionResult result = GetSelectedDepsResult(results, target);

  if (apiResult == CONFLICT && result >= INSTALLED)
    return apiResult;
  return result;
}

RteItem::ConditionResult RteTarget::GetSelectedDepsResult(map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const
{
  RteItem::ConditionResult res = RteItem::IGNORED;
  for (auto ita = m_selectedAggregates.begin(); ita != m_selectedAggregates.end(); ++ita) {
    RteComponentAggregate* a = ita->first;
    if (a && a->IsFiltered() && a->IsSelected()) {
      ConditionResult r = a->GetDepsResult(results, target);
      if (r < res && r > UNDEFINED)
        res = r;
    }
  }
  return res;
}

void RteTarget::SetComponentUsed(RteComponentInstance* ci, int count)
{
  if (!ci)
    return;
  CategorizeComponentInstance(ci, count);
}

void RteTarget::ClearUsedComponents()
{
  m_classes->ClearUsedComponents();
  m_classes->Purge();
}


const map<RteComponentAggregate*, int>& RteTarget::CollectSelectedComponentAggregates()
{
  m_selectedAggregates.clear();
  m_gpdscFileNames.clear();
  CollectSelectedComponentAggregates(m_selectedAggregates);
  // update collection of gpdsc file names
  for (auto it = m_selectedAggregates.begin(); it != m_selectedAggregates.end(); ++it) {
    RteComponentAggregate* a = it->first;
    RteComponent* c = a->GetComponent();
    if (c) {
      string gpdsc = c->GetGpdscFile(this);
      if (!gpdsc.empty())
        m_gpdscFileNames.insert(gpdsc);
    }
  }
  return m_selectedAggregates;
}

void RteTarget::CollectSelectedComponentAggregates(map<RteComponentAggregate*, int>& selectedAggregates) const
{
  m_classes->CollectSelectedComponentAggregates(selectedAggregates);
}

void RteTarget::GetUnSelectedGpdscAggregates(set<RteComponentAggregate*>& unSelectedGpdscAggregates) const
{
  m_classes->GetUnSelectedGpdscAggregates(unSelectedGpdscAggregates);
}

void RteTarget::ClearSelectedComponents()
{
  m_selectedAggregates.clear();
  m_classes->ClearSelectedComponents();
}


void RteTarget::GetSpecificBundledClasses(const map<RteComponentAggregate*, int>& aggregates, map<string, string>& specificClasses)
{
  for (auto it = aggregates.begin(); it != aggregates.end(); ++it) {
    RteComponentAggregate* a = it->first;
    RteComponentInstance* ci = a->GetComponentInstance();
    if (ci && ci->IsTargetSpecific()) {
      const string& bundleName = a->GetCbundleName();
      if (!bundleName.empty()) {
        const string& className = ci->GetCclassName();
        specificClasses[className] = bundleName;
      }
    }
  }
}

void RteTarget::SetSelectionFromTarget(RteTarget* otherTarget)
{
  if (!otherTarget || otherTarget == this)
    return;
  const map<RteComponentAggregate*, int>& otherAggregates = otherTarget->CollectSelectedComponentAggregates();

  map<RteComponentAggregate*, int> savedAggregates; // save this selection
  CollectSelectedComponentAggregates(savedAggregates);

  map<string, string> specificClasses;
  GetSpecificBundledClasses(otherAggregates, specificClasses);
  GetSpecificBundledClasses(savedAggregates, specificClasses);

  ClearSelectedComponents();

  for (auto it = otherAggregates.begin(); it != otherAggregates.end(); ++it) {
    RteComponentAggregate* other = it->first;
    RteComponentInstance* ci = other->GetComponentInstance();
    if (ci && ci->IsTargetSpecific()) {
      continue;
    }
    const string& otherClassName = other->GetCclassName();
    if (specificClasses.find(otherClassName) != specificClasses.end()) {
      continue;
    }
    int count = it->second;
    RteComponent* c = other->GetComponent();
    if (count > 0 && c && IsComponentFiltered(c)) {
      count = other->IsSelected();
      SelectComponent(c, count, false, true);
    } else if (ci) {
      RteComponentAggregate* a = FindComponentAggregate(ci);
      if (a) {
        SelectComponent(a, count, false, true);
      }
    }
  }

  for (auto it = savedAggregates.begin(); it != savedAggregates.end(); ++it) {
    RteComponentAggregate* a = it->first;
    int count = it->second;
    if (!count)
      continue;

    RteComponent* c = a->GetComponent();
    RteComponentInstance* ci = a->GetComponentInstance();
    const string& className = a->GetCclassName();
    if ((ci && ci->IsTargetSpecific()) || specificClasses.find(className) != specificClasses.end()) {
      SelectComponent(a, count, false, true);
    } else if (c && !otherTarget->IsComponentFiltered(c)) {
      SelectComponent(c, count, false, true);
    }
  }
}

void RteTarget::ClearCollections()
{
  m_projectGroups.clear();
  m_fileToComponentInstanceMap.clear();
  m_includePaths.clear();
  m_headers.clear();
  m_deviceHeader.clear();
  m_librarySourcePaths.clear();
  m_libraries.clear();
  m_objects.clear();
  m_docs.clear();
  m_scvdFiles.clear();
  m_RTE_Component_h.clear();
  m_PreIncludeFiles.clear();
  m_PreIncludeGlobal.clear();
  m_PreIncludeLocal.clear();
  m_deviceStartupComponent = NULL;
  m_deviceEnvironment = 0;

  m_defines.clear();
  m_algos.clear();

  m_svd.clear();

  for (auto it = m_availableTemplates.begin(); it != m_availableTemplates.end(); ++it) {
    delete it->second;
  }
  m_availableTemplates.clear();
}

const string& RteTarget::GetVendorString() const
{
  return GetAttribute("Dvendor");
}

void RteTarget::ProcessAttributes() // called from SetAttributes(), AddAttributes(), ClearAttributes() and UpdateFilterModel, sets device
{
  if (m_bDestroy)
    return;
  m_device = 0;
  RteModel *model = GetFilteredModel();
  if (!model)
    return;

  string vendor = GetVendorName();
  string fullDeviceName = GetFullDeviceName();

  m_device = model->GetDevice(fullDeviceName, vendor);

  if (!m_device) {
    return;
  }
  // add Dcore attribute if not present
  if (!HasAttribute("Dcore")) {
    const string& pname = GetProcessorName();
    RteDeviceProperty* p = m_device->GetProcessor(pname);
    if (p) {
      AddAttribute("Dcore", p->GetEffectiveAttribute("Dcore"));
    }
  }
  // resolve board
  string bname = GetAttribute("Bname");
  if (!bname.empty()) {
    const string &rev = HasAttribute("Bversion") ? GetAttribute("Bversion") : GetAttribute("Brevision");
    if (!rev.empty()) {
      bname += " (" + rev + ")";
    }
    SetBoard(model->FindBoard(bname));
  }
};

void RteTarget::AddBoadProperties(RteDeviceItem* device, const string& processorName) {

  // remove all board algos if any: target can only refer to a single board
  for (auto it = m_algos.begin(); it != m_algos.end();) {
    auto itcur = it++;
    const string& algo = *itcur;
    if (algo.find("$$Board") != string::npos) {
      m_algos.erase(itcur);
    }
  }

  RteBoard* board = GetBoard();
  if (!board)
    return;

  // for now only algorithms are added
  Collection<RteItem*> algos;
  for (RteItem* item : board->GetAlgorithms(algos)) {
    const string& pname = item->GetProcessorName();
    if (pname.empty() || pname == processorName) {
      AddAlgorithm(item, board);
    }
  }
}

void RteTarget::AddDeviceProperties(RteDeviceItem* d, const string& processorName)
{
  m_device = d; // ensure we have the actual device (after testing might be changed)
  if (!d)
    return;

  RtePackage *package = d->GetPackage();
  if (package == NULL) { // no package - no path, following info is useless...
    return;
  }

  AddBoadProperties(d, processorName);

  string packagePath = RteUtils::ExtractFilePath(package->GetPackageFileName(), true);
  // get properties for given processor:
  const RteDevicePropertyMap& propMap = d->GetEffectiveProperties(processorName);

  for (auto itpm = propMap.begin(); itpm != propMap.end(); ++itpm) {
    const string& propType = itpm->first; // processor, feature, mamory, etc.
    const list<RteDeviceProperty*>& props = itpm->second;
    for (auto itp = props.begin(); itp != props.end(); ++itp) {
      RteDeviceProperty *p = *itp;
      if (propType == "compile") { // get any attributes, for example compile
        const string& header = p->GetAttribute("header");
        if (!header.empty()) {
          // device header is a special file :
          // we need its name, but should use its path only if no Device.Startup component is available
          m_deviceHeader = RteUtils::ExtractFileName(header);
          RteFile* deviceHeaderFile = NULL;
          if (m_deviceStartupComponent) {
            // does startup component contain the file?
            deviceHeaderFile = FindFile(m_deviceHeader, m_deviceStartupComponent);
          }
          if (deviceHeaderFile) {
            AddFile(m_deviceHeader, RteFile::Category::HEADER, "Device header", m_deviceStartupComponent, deviceHeaderFile);
          } else {
            AddIncludePath(RteUtils::ExtractFilePath(packagePath + header, false), RteFile::Language::LANGUAGE_NONE);
            AddFile(RteUtils::ExtractFileName(header), RteFile::Category::HEADER, "Device header");
          }
        }
        const string& define = p->GetAttribute("define");
        if (!define.empty()) {
          m_defines.insert(define);
        }
        const string& pDefine = p->GetAttribute("Pdefine");
        if (!pDefine.empty()) {
          m_defines.insert(pDefine);
        }
      } else if (propType == "debug") { // get any attributes, for example debug
        const string& svd = p->GetAttribute("svd");
        if (!svd.empty()) {
          m_svd = packagePath + svd;
        }
      } else if (propType == "algorithm") {
        AddAlgorithm(p, d);
      } else if (propType == "environment") {
        if (p->GetName() == "uv") {
          m_deviceEnvironment = p;
        }
      }
    }
  }
}

void RteTarget::AddAlgorithm(RteItem* algo, RteItem* holder)
{
  if (!algo || !holder) {
    return;
  }
  const string& style = algo->GetAttribute("style");
  if (!style.empty() && style != "Keil") {
    return; // ignore non-keil algos for the time being
  }
  string pathName;
  // refer to it in abstract way using board or device name:
  if (dynamic_cast<RteBoard*>(holder)) {
    pathName = "$$Board:";
    pathName += holder->GetName();
  } else {
    pathName = "$$Device:";
    pathName += holder->GetName();
  }
  pathName += "$";
  pathName += algo->GetAttribute("name");
  m_algos.insert(pathName);
}

void RteTarget::CollectComponentSettings(RteComponentInstance* ci)
{
  int count = ci->GetInstanceCount(GetName());
  if (count <= 0)
    return;

  if (!ci->IsApi()) {
    string projectGroup = ci->GetProjectGroupName();
    AddProjectGroup(projectGroup);
  }

  RteComponent* c = ci->GetResolvedComponent(GetName());
  if (!c)
    return;
  if (ci->IsUsedByTarget(GetName())) {
    if (c->IsDeviceStartup())
      m_deviceStartupComponent = c;
    const string& doc = c->GetDocFile();
    if (!doc.empty()) {
      m_docs.insert(doc);
    }
    // collect license files
    RteItem* ls = c->GetLicenseSet();
    if (ls) {
      for (auto lic : ls->GetChildren()) {
        const string& licFile = lic->GetDocFile();
        if (!licFile.empty()) {
          m_docs.insert(licFile);
        }
      }
    }

    CollectPreIncludeStrings(c, count);
  }
  const set<RteFile*>& files = GetFilteredFiles(c);
  if (files.empty())
    return;
  string deviceName = GetFullDeviceName();
  const string& rteFolder = GetRteFolder(ci);
  for (auto itf = files.begin(); itf != files.end(); ++itf) {
    RteFile* f = *itf;
    if (!f)
      continue;
    if (f->IsConfig()) {
      for (int i = 0; i < count; i++) {
        string id = f->GetInstancePathName(deviceName, i, rteFolder);
        AddComponentInstanceForFile(id, ci);
      }
      continue;
    }
    AddFile(f, ci);
  }
}


void RteTarget::CollectPreIncludeStrings(RteComponent* c, int count)
{
  if (c == NULL || count <= 0)
    return;
  string componentComment = "/* ";
  componentComment += c->GetFullDisplayName();
  componentComment += " */\n";
  // collect RTE_Components_h
  string s = RteUtils::ExpandInstancePlaceholders(c->GetItemValue("RTE_Components_h"), count);
  if (!s.empty()) {
    m_RTE_Component_h.insert(componentComment + RteUtils::EnsureLf(s));
  }
  // collect global pre-includes
  s = RteUtils::ExpandInstancePlaceholders(c->GetItemValue("Pre_Include_Global_h"), count);
  if (!s.empty()) {
    m_PreIncludeGlobal.insert(componentComment + RteUtils::EnsureLf(s));
    AddPreIncludeFile("Pre_Include_Global.h", NULL);
  }
  // collect local pre-includes
  s = RteUtils::ExpandInstancePlaceholders(c->GetItemValue("Pre_Include_Local_Component_h"), count);
  if (!s.empty()) {
    string fileName = c->ConstructComponentPreIncludeFileName();
    AddPreIncludeFile(fileName, c);
    m_PreIncludeLocal[c] = componentComment + RteUtils::EnsureLf(s);
  }
}


void RteTarget::CollectClassDocs()
{
  if (!m_classes)
    return;
  const map<string, RteComponentGroup*>& classes = m_classes->GetGroups();
  for (auto it = classes.begin(); it != classes.end(); ++it) {
    RteComponentGroup* g = it->second;
    if (g->IsSelected()) {
      const string& doc = g->GetDocFile();
      if (!doc.empty())
        m_docs.insert(doc);
    }
  }
}

void RteTarget::AddFileInstance(RteFileInstance* fi)
{
  if (!fi || fi->IsRemoved())
    return;
  const string& id = fi->GetInstanceName();
  RteComponentInstance* ci = GetComponentInstanceForFile(id);
  if (!ci)
    ci = fi->GetComponentInstance(GetName());

  RteFile::Category cat = fi->GetCategory();
  string effectivePathName;
  if (fi->IsConfig()) {
    effectivePathName = "./" + id;
  }
  if (fi->IsUsedByTarget(GetName())) {
    RteComponent* c = ci ? ci->GetComponent(GetName()) : NULL;
    if (cat == RteFile::Category::HEADER) {
      effectivePathName = fi->GetIncludeFileName();
      string incPath = "./" + fi->GetIncludePath();
      if (fi->GetScope() != RteFile::Scope::SCOPE_PRIVATE) {
        AddIncludePath(incPath, fi->GetLanguage());
      }
    }
    AddFile(effectivePathName, cat, fi->GetHeaderComment(), c, fi->GetFile(GetName()));
  }
  string groupName = fi->GetProjectGroupName();
  AddProjectGroup(groupName);
  m_projectGroups[groupName][id] = RteFileInfo(fi->GetCategory(), ci, fi);
}

void RteTarget::AddFile(RteFile* f, RteComponentInstance* ci)
{
  if (!ci || !f || f->IsConfig())
    return;

  RteComponent* c = f->GetComponent();
  if (!c) {
    return;
  }

  string id = f->GetOriginalAbsolutePath();
  AddComponentInstanceForFile(id, ci);

  if (f->IsAddToProject()) {
    string groupName = c->GetProjectGroupName();
    AddProjectGroup(groupName);
    m_projectGroups[groupName][id] = RteFileInfo(f->GetCategory(), ci);
  }

  if (!ci->IsUsedByTarget(GetName()))
    return;
  if (f->IsTemplate()) {
    RteFileTemplateCollection* collection = GetTemplateCollection(c);
    if (!collection) {
      collection = new RteFileTemplateCollection(c);
      m_availableTemplates[c] = collection;
    }
    int instanceCount = ci->GetInstanceCount(GetName());
    collection->AddFile(f, instanceCount);
  } else {
    RteFile::Category cat = f->GetCategory();
    string pathName;
    if (cat == RteFile::Category::HEADER) {
      if (f->GetScope() == RteFile::Scope::SCOPE_PRIVATE) {
        AddPrivateIncludePath(f->GetIncludePath(), c, f->GetLanguage());
      } else {
        AddIncludePath(f->GetIncludePath(), f->GetLanguage());
      }
      pathName = f->GetIncludeFileName();
    } else {
      pathName = f->GetOriginalAbsolutePath();
    }
    AddFile(pathName, cat, f->GetHeaderComment(), c, f);
    if (cat == RteFile::Category::LIBRARY)
      f->GetAbsoluteSourcePaths(m_librarySourcePaths);
  }
}


void RteTarget::AddFile(const string& pathName, RteFile::Category cat, const string& comment, RteComponent* c, RteFile* f)
{
  if (pathName.empty())
    return;
  RteFile::Language language = f ? f->GetLanguage() : RteFile::Language::LANGUAGE_NONE;
  switch (cat) {
  case RteFile::Category::HEADER:
  {
    if (!f || f->GetScope() != RteFile::Scope::SCOPE_PRIVATE) {
      m_headers[pathName] = comment;
    }
    break;
  }
  case RteFile::Category::INCLUDE:
  {
    if (f && f->GetScope() == RteFile::Scope::SCOPE_PRIVATE) {
      AddPrivateIncludePath(pathName, c, language);
    } else {
      AddIncludePath(pathName, language);
    }
  }
  break;
  case RteFile::Category::LIBRARY:
    // Note: ED 01.08.2013 - libs are added to project directly, see RteFile::IsAddToProject()
    m_libraries.insert(pathName);
    break;
  case RteFile::Category::OBJECT:
    // Note: ED 01.08.2013 - objs are added to project directly, see RteFile::IsAddToProject()
    m_objects.insert(pathName);
    break;
  case RteFile::Category::SVD:
    m_svd = pathName;
    break;
  case RteFile::Category::PRE_INCLUDE_LOCAL:
    AddPreIncludeFile(pathName, c);
    break;
  case RteFile::Category::PRE_INCLUDE_GLOBAL:
    AddPreIncludeFile(pathName, NULL);
    break;
  default:
  {
    // check for scvd file
    string ext = RteUtils::ExtractFileExtension(pathName);
    if (ext == "scvd") {
      m_scvdFiles[pathName] = c;
    }
  }
  break;
  };
}

void RteTarget::AddPreIncludeFile(const string& pathName, RteComponent* c) {
  if (pathName.empty())
    return;
  auto& preIncludeFiles = m_PreIncludeFiles.try_emplace(c, set<string>()).first->second;
  preIncludeFiles.insert(pathName);
}

const set<string>& RteTarget::GetPreIncludeFiles(RteComponent* c) const
{
  auto it = m_PreIncludeFiles.find(c);
  if (it != m_PreIncludeFiles.end()) {
    return it->second;
  }
  return RteUtils::EMPTY_STRING_SET;
}

const std::set<std::string>& RteTarget::GetIncludePaths(RteFile::Language language) const
{
  return GetPrivateIncludePaths(nullptr, language);
}

void RteTarget::AddIncludePath(const string& path, RteFile::Language language)
{
  InternalAddIncludePath(path, nullptr, language);
}


void RteTarget::AddPrivateIncludePath(const string& path, RteComponent* c, RteFile::Language language)
{
  if (c) {
    InternalAddIncludePath(path, c, language);
  }
}

void RteTarget::InternalAddIncludePath(const string& path, RteComponent* c, RteFile::Language language)
{
  string incpath = NormalizeIncPath(path);
  if (incpath.empty()) {
    return;
  }
  // ensures collections for the language and the component exist
  m_includePaths[c][language].insert(incpath);
}

const std::set<std::string>& RteTarget::GetPrivateIncludePaths(RteComponent* c, RteFile::Language language) const
{
  // get collection specific to component (or global if c == nullptr)
  auto it = m_includePaths.find(c);
  if (it != m_includePaths.end()) {
    auto& pathsMap = it->second;
    auto it1 = pathsMap.find(language);
    if (it1 != pathsMap.end()) {
      return it1->second;
    }
  }
  return RteUtils::EMPTY_STRING_SET;
}

std::set<std::string>& RteTarget::GetEffectivePrivateIncludePaths(std::set<std::string>& includePaths,
  RteComponent* c, RteFile::Language language) const
{
  auto& languagePaths = GetPrivateIncludePaths(c, language);
  includePaths.insert(languagePaths.begin(), languagePaths.end());
  if (language == RteFile::Language::LANGUAGE_C || language == RteFile::Language::LANGUAGE_CPP) {
    GetEffectivePrivateIncludePaths(includePaths,c, RteFile::Language::LANGUAGE_C_CPP);
  }
  if (language != RteFile::Language::LANGUAGE_NONE) {
    GetEffectivePrivateIncludePaths(includePaths, c, RteFile::Language::LANGUAGE_NONE);
  }
  return includePaths;
}

std::set<std::string>& RteTarget::GetEffectiveIncludePaths(std::set<std::string>& includePaths, RteFile::Language language, RteComponent* c) const
{
  GetEffectivePrivateIncludePaths(includePaths, c, language);
  if (c) {
    // combine with global
    GetEffectivePrivateIncludePaths(includePaths, nullptr, language);
  }
  return includePaths;
}

string RteTarget::NormalizeIncPath(const string& path) const
{
  return ReplaceProjectPathWithDotSlash(RteUtils::RemoveTrailingBackslash(path));
}

string RteTarget::ReplaceProjectPathWithDotSlash(const string& path) const
{
  // replace to relative if path starts with project path
  RteProject* proj = GetProject();
  if (proj&& !path.empty()) {
    const string& projPath = proj->GetProjectPath();
    if (!projPath.empty() && path.find(projPath) == 0) {
      return string("./") + path.substr(projPath.length());
    }
  }
  return path;
}


RteFileTemplateCollection* RteTarget::GetTemplateCollection(RteComponent* c) const
{
  auto it = m_availableTemplates.find(c);
  if (it != m_availableTemplates.end())
    return it->second;
  return NULL;
}

const string& RteTarget::GetDeviceEnvironmentString(const string& tag) const
{
  if (m_deviceEnvironment) {
    RteDeviceProperty* p = m_deviceEnvironment->GetEffectiveContentProperty(tag);
    if (p)
      return p->GetText();
  }
  return EMPTY_STRING;
}


bool RteTarget::HasProjectGroup(const string& groupName) const
{
  return m_projectGroups.find(groupName) != m_projectGroups.end();
}


const map<string, RteFileInfo>& RteTarget::GetFilesInProjectGroup(const string& groupName) const
{
  auto it = m_projectGroups.find(groupName);
  if (it != m_projectGroups.end())
    return it->second;
  return EMPTY_STRING_TO_INSTANCE_MAP;
}

bool RteTarget::HasFileInProjectGroup(const string& groupName, const string& file) const
{
  const map<string, RteFileInfo>& files = GetFilesInProjectGroup(groupName);
  return files.find(file) != files.end();
}

string RteTarget::GetFileComment(const string& groupName, const string& file) const
{
  const map<string, RteFileInfo>& files = GetFilesInProjectGroup(groupName);
  auto it = files.find(file);
  if (it != files.end()) {
    RteComponentInstance* ci = it->second.m_ci;
    if (ci) {
      string comment = "(";
      comment += ci->GetShortDisplayName();
      comment += ")";
      return comment;
    }
  }
  return EMPTY_STRING;
}

const set<string>& RteTarget::GetLocalPreIncludes(const string& groupName, const string& file) const
{
  const map<string, RteFileInfo>& files = GetFilesInProjectGroup(groupName);
  auto it = files.find(file);
  if (it != files.end()) {
    RteComponentInstance* ci = it->second.m_ci;
    if (ci) {
      RteComponent* c = ci->GetComponent(GetName());
      if (c) {
        return GetPreIncludeFiles(c);
      }
    }
  }
  return RteUtils::EMPTY_STRING_SET;
}



const RteFileInfo* RteTarget::GetFileInfo(const string& groupName, const string& file) const
{
  const map<string, RteFileInfo>& files = GetFilesInProjectGroup(groupName);
  auto it = files.find(file);
  if (it != files.end()) {
    return &(it->second);
  }
  return NULL;
}


void RteTarget::AddProjectGroup(const string& groupName)
{
  if (!HasProjectGroup(groupName))
    m_projectGroups[groupName] = EMPTY_STRING_TO_INSTANCE_MAP;
}


void RteTarget::ClearFilteredComponents()
{
  m_potentialComponents.clear();
  m_filteredComponents.clear();
  m_filteredBundles.clear();
  m_filteredApis.clear();
  m_filteredFiles.clear();
  m_selectedAggregates.clear();
  m_classes->Clear();
  m_dependencySolver->Clear();
  m_filterContext->Clear();
}


RteComponent* RteTarget::GetComponent(const string& id) const
{
  auto it = m_filteredComponents.find(id);
  if (it != m_filteredComponents.end())
    return it->second;
  return NULL;
}


RteComponent* RteTarget::GetPotentialComponent(const string& id) const
{
  auto it = m_potentialComponents.find(id);
  if (it != m_potentialComponents.end())
    return it->second;
  return NULL;
}


RteComponent* RteTarget::GetLatestPotentialComponent(const string& id) const
{
  for (auto it = m_potentialComponents.begin(); it != m_potentialComponents.end(); ++it) {
    RteComponent* c = it->second;
    if (c->GetComponentID(false) == id)
      return c;
  }
  return NULL;
}

RteApi* RteTarget::GetApi(const map<string, string>& componentAttributes) const
{
  RteProject* p = GetProject();
  if (p) {
    const map<string, RteGpdscInfo*>& gpdscInfos = p->GetGpdscInfos();
    for (auto itg = gpdscInfos.begin(); itg != gpdscInfos.end(); ++itg) {
      RteGpdscInfo* gi = itg->second;
      RtePackage* gpdscPack = gi->GetGpdscPack();
      if (!gpdscPack)
        continue;
      RteApi* a = gpdscPack->GetApi(componentAttributes);
      if (a)
        return a;
    }
  }
  return m_filteredModel->GetApi(componentAttributes);
}

RteApi* RteTarget::GetApi(const string& id) const
{
  RteProject* p = GetProject();
  if (p) {
    const map<string, RteGpdscInfo*>& gpdscInfos = p->GetGpdscInfos();
    for (auto itg = gpdscInfos.begin(); itg != gpdscInfos.end(); ++itg) {
      RteGpdscInfo* gi = itg->second;
      RtePackage* gpdscPack = gi->GetGpdscPack();
      if (!gpdscPack)
        continue;
      RteApi* a = gpdscPack->GetApi(id);
      if (a)
        return a;
    }
  }
  return m_filteredModel->GetApi(id);
}

RteComponent* RteTarget::AddFilteredComponents(RteItem* parentContainer) {
  if (!parentContainer) {
    return nullptr;
  }
  RteComponent* deviceStartup = nullptr;
  for (auto itc : parentContainer->GetChildren()) {
    RteComponent* c = dynamic_cast<RteComponent*>(itc);
    if (c) {
      AddFilteredComponent(c);
    } else {
      c = AddFilteredComponents(itc);
    }
    if (!deviceStartup && c && c->IsDeviceStartup()) {
      deviceStartup = c;
    }
  }
  return deviceStartup;
}

void RteTarget::AddFilteredComponent(RteComponent* c)
{
  /*
  Priority to select component, top is higher:
    1. Component has the attribute "generated" will always have the highest priority.
    2. Component from 'dominate' pack regardless version
       consider component version in case both are dominate
    3. Component from device pack
    4. Component with higher pack version number
  */
  string id = c->GetComponentID(true);
  RtePackage* pack = c->GetPackage();
  RteComponent* inserted = GetComponent(id);
  if (!c->IsGenerated() && inserted && pack) {
    // check generated component
    if (inserted->IsGenerated()) {
      if (inserted->HasAttribute("generator") || c->HasAttribute("generator") || c->GetGpdscFile(this) == inserted->GetGpdscFile(this)) {
        string packPath = inserted->GetPackage()->GetAbsolutePackagePath();
        if (packPath != GetProject()->GetProjectPath()) // default gpdsc cannot be un-selected
          inserted->SetAttribute("selectable", "1"); // sets selectable attribute to allow un-selection of a generated bootstrap component
          // for boot-strap components add doc file and description
        if (inserted->GetDescription().empty()) {
          inserted->SetText(c->GetDescription());
        } if (inserted->GetDocFile().empty()) {
          inserted->AddAttribute("doc", c->GetDocFile(), false);
        }
        // add pack info
        inserted->RemoveChild("package", true);
        RteItem* packInfo = new RteItem("package", inserted);
        packInfo->SetAttributes(pack->GetAttributes());
        inserted->AddChild(packInfo); // add bootstrap pack info as a child.
      }
      return; // generated component always has priority
    }

    // check dominate component
    if (c->Dominates(inserted)) {
      m_filteredComponents[id] = c;
      return;
    }
    if (inserted->Dominates(c))
      return;

    // check device package
    RtePackage* devicePack = GetDevicePackage();
    RtePackage* insertedPack = inserted->GetPackage();
    if (insertedPack == devicePack)
      return; // we already have component from DFP
    if (pack == devicePack) {
      m_filteredComponents[id] = c;
      return; // does not depend on version
    }

    // check pack version, not component version !
    if (VersionCmp::Compare(pack->GetVersionString(), insertedPack->GetVersionString()) < 0)
      return; // the inserted component comes from newer package
  }

  m_filteredComponents[id] = c;
}

void RteTarget::AddPotentialComponent(RteComponent* c)
{
  string id = c->GetComponentID(true);
  RtePackage* pack = c->GetPackage();
  RteComponent* inserted = GetPotentialComponent(id);
  if (inserted && pack) {
    const string& packVersion = pack->GetVersionString();
    const string& insertedPackVersion = inserted->GetPackage()->GetVersionString();
    if (VersionCmp::Compare(packVersion, insertedPackVersion) < 0)
      return; // the inserted component comes from newer package
  }
  m_potentialComponents[id] = c;
}

const RtePackageFilter& RteTarget::GetPackageFilter() const
{
  return m_filteredModel->GetPackageFilter();
}

RtePackageFilter& RteTarget::GetPackageFilter()
{
  return m_filteredModel->GetPackageFilter();
}

void RteTarget::SetPackageFilter(const RtePackageFilter& filter)
{
  m_filteredModel->SetPackageFilter(filter);
}

void RteTarget::UpdateFilterModel()
{
  if (!IsTargetSupported())
    return;

  ClearFilteredComponents();
  m_filteredModel->SetFilterContext(GetFilterContext());
  RteModel* globalModel = GetModel(); // global one
  m_effectiveDevicePackage = m_filteredModel->FilterModel(globalModel, GetDevicePackage());
  if (m_effectiveDevicePackage != GetDevicePackage())
    ProcessAttributes(); // updates device
  FilterComponents();
}

void RteTarget::FilterComponents()
{
  RteComponent* deviceStartup = 0;

  RteProject* p = GetProject();
  if (p) {
    const map<string, RteGpdscInfo*>& gpdscInfos = p->GetGpdscInfos();
    for (auto itg = gpdscInfos.begin(); itg != gpdscInfos.end(); ++itg) {
      RteGpdscInfo* gi = itg->second;
      if (!gi->IsUsedByTarget(GetName()))
        continue;
      RtePackage* gpdscPack = gi->GetGpdscPack();
      if (gpdscPack) {
        deviceStartup = AddFilteredComponents(gpdscPack->GetComponents());
      }
    }
  }

  // fill unique filtered list from filtered model
  const RteComponentMap& componentList = m_filteredModel->GetComponentList();
  RteComponentMap::const_iterator itc;
  for (itc = componentList.begin(); itc != componentList.end(); ++itc) {
    RteComponent* c = itc->second;
    if (deviceStartup && c->IsDeviceStartup())
      continue; // always take device startup from generated project
    ConditionResult r = c->Evaluate(GetFilterContext());
    if (r > FAILED) {
      AddFilteredComponent(c);
    }
  }
  // categorize component, add bundle and filter files
  for (auto [_, c] : m_filteredComponents) {
    RteApi* a = GetApi(c->GetAttributes());
    if (a && m_filteredApis.find(a->GetID()) == m_filteredApis.end()) { // component has an API and the API is not inserted yet
      m_filteredApis[a->GetID()] = a;
      CategorizeComponent(a);
    }
    RteBundle* b = c->GetParentBundle();
    if (b) {
      string id = b->GetBundleShortID();
      if(!contains_key(m_filteredBundles, id))
        m_filteredBundles[id] = b;
    }
    CategorizeComponent(c);
  }

  // add potential components from global model
  RteModel* globalModel = GetModel();

  const RteComponentMap& allComponents = globalModel->GetComponentList();
  // fill unique filtered list
  for (itc = allComponents.begin(); itc != allComponents.end(); ++itc) {
    RteComponent* c = itc->second;
    RtePackage* pack = c->GetPackage();
    if (GetPackageFilter().IsPackageFiltered(pack))
      continue; // already processed
    ConditionResult r = c->Evaluate(GetFilterContext());
    if (r > FAILED) {
      AddPotentialComponent(c);
    }
  }
  CollectSelectedComponentAggregates();
  EvaluateComponentDependencies();
}

void RteTarget::AddFilteredFiles(RteComponent* c, const set<RteFile*>& files)
{
  m_filteredFiles[c] = files;
}

const set<RteFile*>& RteTarget::GetFilteredFiles(RteComponent* c) const
{
  auto it = m_filteredFiles.find(c);
  if (it != m_filteredFiles.end())
    return it->second;
  static set<RteFile*> EMPTY_FILE_SET;
  return EMPTY_FILE_SET;
}

RteFile* RteTarget::GetFile(const string& name, RteComponent* c) const
{
  // returns file if it is filtered for given target
  const set<RteFile*>& filteredFiles = GetFilteredFiles(c);
  for (auto itf = filteredFiles.begin(); itf != filteredFiles.end(); ++itf) {
    RteFile* f = *itf;
    if (f && f->GetName() == name)
      return f;
  }
  return NULL;
}

RteFile* RteTarget::FindFile(const string& fileName, RteComponent* c) const
{
  const set<RteFile*>& filteredFiles = GetFilteredFiles(c);
  for (auto itf = filteredFiles.begin(); itf != filteredFiles.end(); ++itf) {
    RteFile* f = *itf;
    if (f && RteUtils::ExtractFileName(f->GetName()) == fileName)
      return f;
  }
  return NULL;
}

const std::string& RteTarget::GetRteFolder() const {
  RteProject* rteProject = GetProject();
  if (rteProject) {
    return rteProject->GetRteFolder();
  }
  return RteProject::DEFAULT_RTE_FOLDER;
}

const std::string& RteTarget::GetRteFolder(const RteComponentInstance* ci) const {
  if (ci && !ci->GetRteFolder().empty()) {
    return ci->GetRteFolder();
  }
  return GetRteFolder();
}

RteFile* RteTarget::GetFile(const RteFileInstance* fi, RteComponent* c) const
{
  return GetFile(fi, c, GetRteFolder());
}


RteFile* RteTarget::GetFile(const RteFileInstance* fi, RteComponent* c, const std::string& rteFolder) const
{
  if (!fi) {
    return nullptr;
  }
  const set<RteFile*>& filteredFiles = GetFilteredFiles(c);
  const string& deviceName = GetFullDeviceName();
  int index = fi->GetInstanceIndex();
  const string& instanceName = fi->GetInstanceName();
  for (auto itf = filteredFiles.begin(); itf != filteredFiles.end(); ++itf) {
    RteFile* f = *itf;
    if (f && f->GetInstancePathName(deviceName, index, rteFolder) == instanceName) {
      return f;
    }
  }
  return nullptr;
}

void RteTarget::EvaluateComponentDependencies()
{
  if (!IsTargetSupported())
    return;
  m_dependencySolver->EvaluateDependencies();
}

void RteTarget::CollectFilteredFiles()
{
  m_filteredFiles.clear(); // filtered files can depend on selected components
  map<RteComponentAggregate*, int> components;
  CollectSelectedComponentAggregates(components);
  for (auto itc = components.begin(); itc != components.end(); ++itc) {
    RteComponentAggregate* a = itc->first;
    RteComponent* c = a->GetComponent();
    if (c) {
      c->FilterFiles(this);
      RteApi* api = c->GetApi(this, true);
      if (api) {
        api->FilterFiles(this);
      }
    }
  }
}


void RteTarget::CategorizeComponent(RteComponent* c)
{
  const string& className = c->GetCclassName();
  const string& groupName = c->GetCgroupName();
  const string& subName = c->GetCsubName();
  RteComponentGroup* group = m_classes->EnsureGroup(className);

  if (!subName.empty() || c->IsApi() || c->HasApi(this)) {
    group = group->EnsureGroup(groupName);
  }
  group->AddComponent(c);
}


void RteTarget::CategorizeComponentInstance(RteComponentInstance* ci, int count)
{
  const string& className = ci->GetCclassName();
  RteItem* effectiveItem = ci->GetEffectiveItem(GetName());

  const string& groupName = effectiveItem->GetCgroupName();
  const string& subName = effectiveItem->GetCsubName();
  RteComponentGroup* group = NULL;
  if (count > 0) {
    group = m_classes->EnsureGroup(className);
    if (!subName.empty() || ci->IsApi() || ci->GetApiInstance()) {
      // in case of missing API it could be that we already have an aggregate on this level
      string aggregateId = ci->GetComponentAggregateID();
      RteComponentAggregate* a = group->GetComponentAggregate(aggregateId);
      group = group->EnsureGroup(groupName);
      // move the aggregate to new group
      if (a) {
        a->Reparent(group);
      }

    }
  } else {
    group = m_classes->GetGroup(className);
    if (group && (!subName.empty() || ci->IsApi() || ci->GetApiInstance())) {
      group = group->GetGroup(groupName);
    }
  }

  if (group)
    group->AddComponentInstance(ci, count);
}


RteComponentClass* RteTarget::GetComponentClass(const string& name) const
{
  return m_classes->FindComponentClass(name);
}

RteComponentGroup* RteTarget::GetComponentGroup(RteComponent* c) const
{
  return m_classes->GetComponentGroup(c);
}

RteComponentAggregate* RteTarget::GetComponentAggregate(RteComponent* c) const
{
  return m_classes->GetComponentAggregate(c);
}

RteComponentAggregate* RteTarget::GetComponentAggregate(const string& id) const
{
  return m_classes->GetComponentAggregate(id);
}

RteComponentAggregate* RteTarget::FindComponentAggregate(RteComponentInstance* ci) const
{
  if (!ci)
    return NULL;
  return m_classes->FindComponentAggregate(ci);
}


RteComponent* RteTarget::GetLatestComponent(RteComponentInstance* ci) const
{
  if (ci) {
    RteComponentAggregate* a = GetComponentAggregate(ci->GetComponentAggregateID());
    if (a)
      return a->GetLatestComponent(ci->GetCvariantName());
  }
  return NULL;
}


RteComponent* RteTarget::GetCMSISCoreComponent() const
{
  static string cmsisCoreAggregateID("ARM::CMSIS.CORE");
  RteComponentAggregate* a = GetComponentAggregate(cmsisCoreAggregateID);
  if (a)
    return a->GetLatestComponent("");
  return NULL;
}


string RteTarget::GetCMSISCoreIncludePath() const
{
  RteComponent* c = GetCMSISCoreComponent();
  if (c) {
    RteFileContainer* fc = c->GetFileContainer();
    if (fc) {
      for (auto child : fc->GetChildren()) {
        RteFile* f = dynamic_cast<RteFile*>(child);
        if (f && f->GetCategory() == RteFile::Category::INCLUDE) {
          string inc = f->GetOriginalAbsolutePath();
          return inc;
        }
      }
    }
  }
  return EMPTY_STRING;
}


RteComponent* RteTarget::ResolveComponent(RteComponentInstance* ci) const
{
  if (ci->IsApi()) {
    return m_filteredModel->GetApi(ci->GetAttributes());
  }

  VersionCmp::MatchMode mode = ci->GetVersionMatchMode(GetName());
  RteComponent* c = NULL;
  if (mode == VersionCmp::MatchMode::ENFORCED_VERSION) {
    RteComponentList lst;
    return GetFilteredModel()->FindComponents(*ci, lst);
  } else if (mode == VersionCmp::MatchMode::FIXED_VERSION) {
    c = GetComponent(ci->GetComponentID(true));
  } else {
    c = GetLatestComponent(ci);
  }
  if (c)
    return c;

  if (ci->GetCbundleName().empty()) {
    // try to find a component with a bundle
    RteComponentAggregate* a = m_classes->FindComponentAggregate(ci);
    if (a) {
      if (mode <= VersionCmp::MatchMode::FIXED_VERSION)
        c = a->GetComponent(ci->GetCvariantName(), ci->GetVersionString());
      else
        c = a->GetLatestComponent(ci->GetCvariantName());
    }
  }
  return c;
}


RteComponent* RteTarget::GetPotentialComponent(RteComponentInstance* ci) const
{
  if (GetPackageFilter().IsPackageSelected(ci->GetPackageID(true)))
    return NULL; // required pack is not available => no need to resolve it

  if (ci->IsApi())
    return GetModel()->GetApi(ci->GetComponentUniqueID());

  VersionCmp::MatchMode mode = ci->GetVersionMatchMode(GetName());
  RteComponent* c = NULL;
  if (mode == VersionCmp::MatchMode::FIXED_VERSION) {
    c = GetPotentialComponent(ci->GetComponentID(true));
  } else {
    c = GetLatestPotentialComponent(ci->GetComponentID(false));
  }
  return c;
}

RteItem::ConditionResult RteTarget::GetComponentsForApi(RteApi* api, set<RteComponent*>& components, bool selectedOnly) const
{
  if (!api) {
    return MISSING_API;
  }
  set<RteApi*> apiVersions;
  bool exclusive = api->IsExclusive();
  ConditionResult result = MISSING;
  auto& apiAttributes = api->GetAttributes();
  int nSelected = 0;
  for (auto [id, c] : m_filteredComponents) {
    if (c->MatchComponentAttributes(apiAttributes, false)) {
      if (IsComponentSelected(c)) {
        components.insert(c);
        nSelected++;
        apiVersions.insert(GetApi(c->GetAttributes()));
        if (exclusive && nSelected > 1) {
          result = CONFLICT;
        } else if (apiVersions.size() > 1) { //effectively means different API version
          result = CONFLICT;
        } else if (result == MISSING) {
          result = FULFILLED;
        }
      } else if (result == MISSING) {
        result = INSTALLED;
        if (!selectedOnly)
          components.insert(c);
      }
    }
  }
  return result;
}

void RteTarget::AddMissingPackId(const string& pack, const string& url)
{
  if (pack.empty())
    return; // device pack id is missing for old projects
  auto it = t_missingPackIds.find(pack);
  if (it == t_missingPackIds.end() || it->second.empty())
    t_missingPackIds[pack] = url;
}

bool RteTarget::IsPackMissing(const string& pack)
{
  return t_missingPackIds.find(pack) != t_missingPackIds.end();
}

std::string RteTarget::GetDeviceFolder() const
{
  string deviceName = WildCards::ToX(GetFullDeviceName());
  return string("Device/") + deviceName;
}

std::string RteTarget::GetRegionsHeader() const
{
  string deviceName = WildCards::ToX(GetFullDeviceName());
  string boardName = WildCards::ToX(GetAttribute("Bname"));
  string filename = boardName.empty() ? deviceName : boardName;
  return GetDeviceFolder() + "/regions_" + filename + ".h";
}

std::pair<std::string, std::string> RteTarget::GetAccessAttributes(RteItem* mem) const
{
  return {
    string(mem->IsReadAccess() ? "r" : "") +
          (mem->IsWriteAccess() ? "w" : "") +
          (mem->IsExecuteAccess() ? "x" : ""),
    string(mem->IsPeripheralAccess() ? "p" : "") +
          (mem->IsSecureAccess() ? "s" : "") +
          (mem->IsNonSecureAccess() ? "n" : "") +
          (mem->IsCallableAccess() ? "c" : "")
  };
}

std::string RteTarget::GenerateMemoryRegionContent(const std::vector<RteItem*> memVec, const std::string& id, const std::string& dfp) const
{
  string pack, access, name, start, size;
  bool unused = memVec.empty();
  if (!unused) {
    pack = memVec.front()->GetPackageID() == dfp ? "DFP" : "BSP";
    access = GetAccessAttributes(memVec.front()).first;
    for (const auto& mem : memVec) {
      name += (mem == memVec.front() ? "" : "+") + mem->GetName();
    }
    start = memVec.front()->GetAttribute("start");
    unsigned int decSize = 0;
    ostringstream hexSize;
    for (const auto& mem : memVec) {
      decSize += stoul(mem->GetAttribute("size"), nullptr, 16);
    }
    hexSize << "0x" << uppercase << setfill('0') << setw(8) << hex << decSize;
    size = hexSize.str();
  }
  ostringstream oss;
  oss << "// <h> " << id << " (" << (unused ? "unused" :
    "is " + access + " memory: " + name + " from " + pack) << ")" << RteUtils::LF_STRING;
  oss << "//   <o> Base address <0x0-0xFFFFFFFF:8>" << RteUtils::LF_STRING;
  oss << "//   <i> Defines base address of memory region." << (unused ? "" : " Default: " + start) << RteUtils::LF_STRING;
  if (id == "__ROM0") {
    oss << "//   <i> Contains Startup and Vector Table" << RteUtils::LF_STRING;
  }
  if (id == "__RAM0") {
    oss << "//   <i> Contains uninitialized RAM, Stack, and Heap" << RteUtils::LF_STRING;
  }
  oss << "#define " << id << "_BASE " << (unused ? "0" : start) << RteUtils::LF_STRING;
  oss << "//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>" << RteUtils::LF_STRING;
  oss << "//   <i> Defines size of memory region." << (unused ? "" : " Default: " + size) << RteUtils::LF_STRING;
  oss << "#define " << id << "_SIZE " << (unused ? "0" : size) << RteUtils::LF_STRING;
  oss << "// </h>" << RteUtils::LF_STRING;
  oss << RteUtils::LF_STRING;
  return oss.str();
}

std::string RteTarget::GenerateRegionsHeaderContent() const
{
  // collect device memory data
  RteDeviceItem* device = GetDevice();
  if (!device) {
    return EMPTY_STRING;
  }

  // get memory collections from device and board
  Collection<RteDeviceProperty*> deviceMemCollection;
  Collection<RteItem*> boardMemCollection;
  deviceMemCollection = device->GetEffectiveProperties("memory", GetProcessorName());
  RteBoard* board = GetBoard();
  if (board) {
    board->GetMemories(boardMemCollection);
  }

  // init memory regions
  unsigned int totalRW = 0;
  map<string, vector<RteItem*>> memRO = { {"__ROM0",{}}, {"__ROM1",{}}, {"__ROM2",{}}, {"__ROM3",{}} };
  map<string, vector<RteItem*>> memRW = { {"__RAM0",{}}, {"__RAM1",{}}, {"__RAM2",{}}, {"__RAM3",{}} };
  vector<RteItem*> notAllocated;
  auto init = [&](auto collection) {
    for (auto mem : collection) {
      if (mem->GetAttributeAsBool("default")) {
        if (mem->IsWriteAccess()) {
          totalRW += stoul(mem->GetAttribute("size"), nullptr, 16);
        }
        if (memRW.at("__RAM0").empty() && mem->IsWriteAccess() && mem->GetAttributeAsBool("uninit")) {
          memRW.at("__RAM0").push_back(mem);
          continue;
        }
        if (memRO.at("__ROM0").empty() && mem->IsExecuteAccess() && mem->GetAttributeAsBool("startup")) {
          memRO.at("__ROM0").push_back(mem);
          continue;
        }
      }
      notAllocated.push_back(mem);
    }
  };
  init(deviceMemCollection);
  init(boardMemCollection);

  // allocate remaining memory regions
  for (auto it = notAllocated.begin(); it < notAllocated.end();) {
    auto mem = *it;
    bool allocated = false;
    auto* dst = mem->IsWriteAccess() ? &memRW : mem->IsExecuteAccess() ? &memRO : nullptr;
    if (dst && mem->GetAttributeAsBool("default")) {
      for (auto& [_, alloc] : *dst) {
        if (alloc.empty()) {
          // free slot found
          alloc.push_back(mem);
          allocated = true;
          break;
        } else {
          // search contiguous memory region (same pack, same access)
          if ((mem->GetPackageID() == alloc.back()->GetPackageID()) &&
            GetAccessAttributes(mem) == GetAccessAttributes(alloc.back()) &&
            stoul(mem->GetAttribute("start"), nullptr, 16) == stoul(alloc.back()->GetAttribute("start"), nullptr, 16) +
            stoul(alloc.back()->GetAttribute("size"), nullptr, 16)) {
            alloc.push_back(mem);
            allocated = true;
            break;
          }
        }
      }
    }
    if (allocated) {
      it = notAllocated.erase(it);
    } else {
      it++;
    }
  }

  ostringstream  oss;
  oss <<  RteUtils::LF_STRING;
  oss << "//-------- <<< Use Configuration Wizard in Context Menu >>> --------------------" << RteUtils::LF_STRING;
  oss << "//------ With VS Code: Open Preview for Configuration Wizard -------------------" << RteUtils::LF_STRING;
  oss <<  RteUtils::LF_STRING;

  oss << "// <n> Auto-generated using information from packs" << RteUtils::LF_STRING;
  oss << "// <i> Device Family Pack (DFP):   " << device->GetPackageID(true) << RteUtils::LF_STRING;
  if (board) {
    oss << "// <i> Board Support Pack (BSP):   " << board->GetPackageID(true) << RteUtils::LF_STRING;
  }
  oss << RteUtils::LF_STRING;

  oss << "// <h> ROM Configuration"   << RteUtils::LF_STRING;
  oss << "// =======================" << RteUtils::LF_STRING;
  for (const auto& [id, mem] : memRO) {
    oss << GenerateMemoryRegionContent(mem, id, device->GetPackageID());
  }
  oss << "// </h>" << RteUtils::LF_STRING;
  oss << RteUtils::LF_STRING;

  oss << "// <h> RAM Configuration" << RteUtils::LF_STRING;
  oss << "// =======================" << RteUtils::LF_STRING;
  for (const auto& [id, mem] : memRW) {
    oss << GenerateMemoryRegionContent(mem, id, device->GetPackageID());
  }
  oss << "// </h>" << RteUtils::LF_STRING;
  oss << RteUtils::LF_STRING;

  oss << "// <h> Stack / Heap Configuration" << RteUtils::LF_STRING;
  oss << "//   <o0> Stack Size (in Bytes) <0x0-0xFFFFFFFF:8>" << RteUtils::LF_STRING;
  oss << "//   <o1> Heap Size (in Bytes) <0x0-0xFFFFFFFF:8>" << RteUtils::LF_STRING;
  oss << "#define __STACK_SIZE 0x00000200" << RteUtils::LF_STRING;
  oss << "#define __HEAP_SIZE " << (totalRW >= 6144 ? "0x00000C00" : "0x00000000") << RteUtils::LF_STRING;
  oss << "// </h>" << RteUtils::LF_STRING;

  if (!notAllocated.empty()) {
    oss << RteUtils::LF_STRING << "// <n> Resources that are not allocated to linker regions" << RteUtils::LF_STRING;
    size_t maxNameLength = 0;
    for (const auto& mem : notAllocated) {
      size_t nameLength = mem->GetName().length();
      if (nameLength > maxNameLength) {
        maxNameLength = nameLength;
      }
    }
    for (const auto& mem : notAllocated) {
      oss << "// <i> ";
      oss << left << setw(10) << GetAccessAttributes(mem).first + (mem->IsWriteAccess() ? " RAM:" : " ROM:");
      oss << left << setw(maxNameLength + 12) << mem->GetName() + " from" + (mem->GetPackageID() == device->GetPackageID() ? " DFP:" : " BSP:");
      oss << "BASE: " << mem->GetAttribute("start") << "  SIZE: " << mem->GetAttribute("size");
      oss << (mem->GetProcessorName().empty() ? "" : "  Pname: " + mem->GetProcessorName()) << RteUtils::LF_STRING;
    }
  }

  return oss.str();
}

bool RteTarget::GenerateRegionsHeader(const string& directory)
{
  string content = GenerateRegionsHeaderContent();
  if (content.empty()) {
    return false;
  }
  return GenerateRteHeaderFile(GetRegionsHeader(), content, true, directory);
}

bool RteTarget::GenerateRteHeaders() {
  if (!GenerateRTEComponentsH()) {
    return false;
  }

  string content;
  const set<string>& strings = GetGlobalPreIncludeStrings();
  for (auto s : strings) {
    content += s + RteUtils::LF_STRING;
  }

  if (!content.empty()) {
    GenerateRteHeaderFile("Pre_Include_Global.h", content);
  }

  const map<RteComponent*, string>& locals = GetLocalPreIncludeStrings();
  for (auto entry : locals) {
    RteComponent* c = entry.first;
    if (!c || entry.second.empty()) {
      continue;
    }
    string fileName = c->ConstructComponentPreIncludeFileName();
    GenerateRteHeaderFile(fileName, entry.second);
  }
  return true;
}

bool RteTarget::GenerateRTEComponentsH() {

  if(GetSelectedComponentAggregates().empty()) {
    return true;  // no components selected
  }
  string content;
  const string& devheader = GetDeviceHeader();
  if (!devheader.empty()) {            // found device header file.
    content += szDevHdr;
    content += "\"" + devheader + "\"" + RteUtils::LF_STRING + RteUtils::LF_STRING;
  }

  //---------------------------------------------------
  const set<string>& strings = GetRteComponentHstrings();
  for (auto s : strings) {
    s = RteUtils::RemoveLeadingSpaces(s);
    content += s + RteUtils::LF_STRING;
  }
  return GenerateRteHeaderFile("RTE_Components.h", content);
}

bool RteTarget::GenerateRteHeaderFile(const string& headerName, const string& content, bool bRegionsHeader, const std::string& directory) {

  RteProject *project = GetProject();
  if (!project)
    return false;
  string path = !directory.empty() ? directory : project->GetProjectPath();

  string headerFile = bRegionsHeader ? path + headerName : project->GetRteHeader(headerName, GetName(), path);

  if (bRegionsHeader && RteFsUtils::Exists(headerFile))
    return true; // nothing to do

  if (!RteFsUtils::MakeSureFilePath(headerFile))
    return false;

  string HEADER_H;
  string headerFileName = RteUtils::ExtractFileName(headerName);
  for (string::size_type pos = 0; pos < headerFileName.length(); pos++) {
    char ch = toupper(headerFileName[pos]);
    if (ch == '.' || ch == '-')
      ch = '_';
    HEADER_H += ch;
  }
  // construct head comment
  ostringstream  oss;
  if (!bRegionsHeader) {
    RteCallback* callback = GetCallback();
    if (!callback) {
      return false;
    }

    bool foundToolInfo = false;
    auto kernel = callback->GetRteKernel();
    if (kernel) {
      auto toolInfo = kernel->GetToolInfo();
      auto name = toolInfo.GetAttribute("name");
      auto version = toolInfo.GetAttribute("version");
      if (name != RteUtils::EMPTY_STRING && version != RteUtils::EMPTY_STRING) {
        string capToolName = name;
        transform(capToolName.begin(), capToolName.end(), capToolName.begin(), ::toupper);
        oss << "/*" << RteUtils::LF_STRING;
        oss << " * " << capToolName << " generated file: DO NOT EDIT!" << RteUtils::LF_STRING;
        oss << " * Generated by: " << name << " version " << version << RteUtils::LF_STRING;
        oss << " *" << RteUtils::LF_STRING;
        foundToolInfo = true;
      }
    }

    if (!foundToolInfo) {
      oss << szDefaultRteCh;
    }
    oss << " * Project: '" << project->GetName() << "\' " << RteUtils::LF_STRING;
    oss << " * Target:  '" << GetName() << "\' " << RteUtils::LF_STRING;
    oss << " */" << RteUtils::LF_STRING << RteUtils::LF_STRING;
  }
  // content
  oss << "#ifndef " << HEADER_H << RteUtils::LF_STRING;
  oss << "#define " << HEADER_H << RteUtils::LF_STRING << RteUtils::LF_STRING;
  oss << content << RteUtils::LF_STRING;
  oss << RteUtils::LF_STRING;
  oss << "#endif /* " << HEADER_H << " */" << RteUtils::LF_STRING;

  string str = oss.str();
  string fileBuf;

  // check if file has been changed
  RteFsUtils::ReadFile(headerFile, fileBuf);
  auto fetchContent = [](const string& input) {
    // read the content excluding header info
    string outStr = input;
    size_t pos = outStr.find("#ifndef ", 0);
    if (pos != std::string::npos) {
      outStr = outStr.substr(pos);
    }
    return outStr;
  };
  str = fetchContent(str);
  fileBuf = fetchContent(fileBuf);

  if (fileBuf.compare(str) == 0) {
    return true;
  }

  // file does not exist or its content is different
  RteFsUtils::CopyBufferToFile(headerFile, oss.str(), false); // write file

  return true;
}
// End of RteTarget.cpp
