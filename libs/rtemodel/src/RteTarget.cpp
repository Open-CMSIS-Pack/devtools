/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteTarget.cpp
* @brief CMSIS RTE Data Model filtering
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteTarget.h"

#include "RteProject.h"
#include "RteInstance.h"
#include "RteModel.h"
#include "RteFsUtils.h"

#include <fstream>
#include <sstream>

using namespace std;

static const string szDevHdr = "\n"   \
"/*\n"                                 \
" * Define the Device Header File: \n" \
" */\n"                                \
"#define CMSIS_device_header "; // appends device header

static const string carRet = "\n";

static const string szRteCh = "\n"                   \
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

void RteTarget::ClearMissingPacks()
{
  t_missingPackIds.clear();
}

RteBoard* RteTarget::FindBoard(const string& displayName) const
{
  if (m_filteredModel) {
    return m_filteredModel->FindCompatibleBoard(displayName, GetDevice());
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

  for (auto it = m_filteredComponents.begin(); it != m_filteredComponents.end(); it++) {
    if (it->second == c)
      return true;
  }
  return false;
}

RteItem::ConditionResult RteTarget::GetComponents(const map<string, string>& componentAttributes, set<RteComponent*>& components) const
{
  RteItem::ConditionResult result = RteItem::MISSING;
  for (auto it = m_filteredComponents.begin(); it != m_filteredComponents.end(); it++) {
    RteComponent* c = it->second;
    if (c->HasComponentAttributes(componentAttributes)) {
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


RteItem::ConditionResult RteTarget::GetComponentAggregates(const RteAttributes& componentAttributes, set<RteComponentAggregate*>& aggregates) const
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
  if (!IsComponentFiltered(c))
    count = 0;

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
  for (auto ita = m_filteredApis.begin(); ita != m_filteredApis.end(); ita++) {
    RteApi* api = ita->second;
    if (!api)
      continue;
    set<RteComponent*> components;
    RteItem::ConditionResult r = GetComponentsForApi(api, components, true);
    if (r == CONFLICT) {
      apiResult = r;
      RteDependencyResult depRes(api, r);
      for (set<RteComponent*>::iterator itc = components.begin(); itc != components.end(); itc++) {
        RteComponent* c = *itc;
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
  for (auto ita = m_selectedAggregates.begin(); ita != m_selectedAggregates.end(); ita++) {
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
  for (auto it = m_selectedAggregates.begin(); it != m_selectedAggregates.end(); it++) {
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
  for (auto it = aggregates.begin(); it != aggregates.end(); it++) {
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

  for (auto it = otherAggregates.begin(); it != otherAggregates.end(); it++) {
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

  for (auto it = savedAggregates.begin(); it != savedAggregates.end(); it++) {
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

  for (auto it = m_availableTemplates.begin(); it != m_availableTemplates.end(); it++) {
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
  // add Dcore attribute as if not present
  if(!m_device || HasAttribute("Dcore")) {
    return;
  }
  const string& pname = GetProcessorName();
  RteDeviceProperty* p = m_device->GetProcessor(pname);
  if(p) {
    AddAttribute("Dcore", p->GetEffectiveAttribute("Dcore"));
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

  // for now only algoritms are added
  for (RteItem* item : board->GetChildren()) {
    if (item && item->GetTag() == "algorithm") {
      const string& pname = item->GetProcessorName();
      if (pname.empty() || pname == processorName) {
        AddAlgorithm(item, board);
      }
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
    for (auto itp = props.begin(); itp != props.end(); itp++) {
      RteDeviceProperty *p = *itp;
      if (propType == "compile") { // get any attributes, for example compile
        const string& header = p->GetAttribute("header");
        if (!header.empty()) {
          // device header is a special file :
          // we need its name, but should use its path only if no Device.Startup component is vavailable
          m_deviceHeader = RteUtils::ExtractFileName(header);
          RteFile* deviceHeaderFile = NULL;
          if (m_deviceStartupComponent) {
            // does startup component contain the file?
            deviceHeaderFile = FindFile(m_deviceHeader, m_deviceStartupComponent);
          }
          if (deviceHeaderFile) {
            AddFile(m_deviceHeader, RteFile::HEADER, "Device header");
          } else {
            AddIncludePath(RteUtils::ExtractFilePath(packagePath + header, false));
            AddFile(RteUtils::ExtractFileName(header), RteFile::HEADER, "Device header");
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
    if (!doc.empty())
      m_docs.insert(doc);
    CollectPreIncludeStrings(c, count);
  }
  const set<RteFile*>& files = GetFilteredFiles(c);
  if (files.empty())
    return;
  string deviceName = GetFullDeviceName();
  for (auto itf = files.begin(); itf != files.end(); itf++) {
    RteFile* f = *itf;
    if (!f)
      continue;
    if (f->IsConfig()) {
      for (int i = 0; i < count; i++) {
        string id = f->GetInstancePathName(deviceName, i);
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
    m_RTE_Component_h.insert(componentComment + RteUtils::EnsureCrLf(s));
  }
  // collect global pre-includes
  s = RteUtils::ExpandInstancePlaceholders(c->GetItemValue("Pre_Include_Global_h"), count);
  if (!s.empty()) {
    m_PreIncludeGlobal.insert(componentComment + RteUtils::EnsureCrLf(s));
    AddPreIncludeFile("Pre_Include_Global.h", NULL);
  }
  // collect local pre-includes
  s = RteUtils::ExpandInstancePlaceholders(c->GetItemValue("Pre_Include_Local_Component_h"), count);
  if (!s.empty()) {
    string fileName = c->ConstructComponentPreIncludeFileName();
    AddPreIncludeFile(fileName, c);
    m_PreIncludeLocal[c] = componentComment + RteUtils::EnsureCrLf(s);
  }
}



void RteTarget::CollectClassDocs()
{
  if (!m_classes)
    return;
  const map<string, RteComponentGroup*>& classes = m_classes->GetGroups();
  for (auto it = classes.begin(); it != classes.end(); it++) {
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
    if (cat == RteFile::HEADER) {
      string incPath = "./" + fi->GetIncludePath();
      AddIncludePath(incPath);
      effectivePathName = fi->GetIncludeFileName();
    }
    RteComponent* c = ci ? ci->GetComponent(GetName()) : NULL;
    AddFile(effectivePathName, cat, fi->GetHeaderComment(), c);
  }
  string groupName = fi->GetProjectGroupName();
  AddProjectGroup(groupName);
  m_projectGroups[groupName][id] = RteFileInfo(fi->GetCategory(), ci, fi);
}

void RteTarget::AddFile(RteFile* f, RteComponentInstance* ci)
{
  if (!ci || !f || f->IsConfig())
    return;

  string id = f->GetOriginalAbsolutePath();
  AddComponentInstanceForFile(id, ci);

  RteComponent* c = f->GetComponent();
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
    if (cat == RteFile::HEADER) {
      AddIncludePath(f->GetIncludePath());
      pathName = f->GetIncludeFileName();
    } else {
      pathName = f->GetOriginalAbsolutePath();
    }
    AddFile(pathName, cat, c->GetAggregateDisplayName(), c);
    if (cat == RteFile::LIBRARY)
      f->GetAbsoluteSourcePaths(m_librarySourcePaths);
  }
}


void RteTarget::AddFile(const string& pathName, RteFile::Category cat, const string& comment, RteComponent* c)
{
  if (pathName.empty())
    return;
  switch (cat) {
  case RteFile::HEADER:
  {
    m_headers[pathName] = comment;
    break;
  }
  case RteFile::INCLUDE:
  {
    string incpath = RteUtils::RemoveTrailingBackslash(pathName);
    AddIncludePath(incpath);
  }
  break;
  case RteFile::LIBRARY:
    // Note: ED 01.08.2013 - libs are added to project directly, see RteFile::IsAddToProject()
    m_libraries.insert(pathName);
    break;
  case RteFile::OBJECT:
    // Note: ED 01.08.2013 - objs are added to project directly, see RteFile::IsAddToProject()
    m_objects.insert(pathName);
    break;
  case RteFile::SVD:
    m_svd = pathName;
    break;
  case RteFile::PRE_INCLUDE_LOCAL:
    AddPreIncludeFile(pathName, c);
    break;
  case RteFile::PRE_INCLUDE_GLOBAL:
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
  auto it = m_PreIncludeFiles.find(c);
  if (it == m_PreIncludeFiles.end()) {
    m_PreIncludeFiles[c] = set<string>();
    it = m_PreIncludeFiles.find(c);
  }
  it->second.insert(pathName);
}

const set<string>& RteTarget::GetPreIncludeFiles(RteComponent* c) const
{
  auto it = m_PreIncludeFiles.find(c);
  if (it != m_PreIncludeFiles.end()) {
    return it->second;
  }
  return RteUtils::EMPTY_STRING_SET;
}


void RteTarget::AddIncludePath(const string& path) {

  string incpath = RteUtils::RemoveTrailingBackslash(path);
  if (incpath.empty())
    return;
  // replace to relative if an include path starts with project path
  RteProject* proj = GetProject();
  if (proj) {
    const string& projPath = proj->GetProjectPath();
    if (!projPath.empty() && incpath.find(projPath) == 0) {
      incpath = incpath.replace(0, projPath.length(), "./");
    }
  }
  m_includePaths.insert(incpath);
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
  for (auto it = m_potentialComponents.begin(); it != m_potentialComponents.end(); it++) {
    RteComponent* c = it->second;
    if (c->GetComponentID(false) == id)
      return c;
  }
  return NULL;
}

RteApi* RteTarget::GetApi(const map<string, string>& componentAttributes) const
{
  RteProject* p = GetProject();
  const map<string, RteGpdscInfo*>& gpdscInfos = p->GetGpdscInfos();
  for (auto itg = gpdscInfos.begin(); itg != gpdscInfos.end(); itg++) {
    RteGpdscInfo* gi = itg->second;
    RteModel* m = gi->GetGeneratorModel();
    if (!m)
      continue;
    RteApi* a = m->GetApi(componentAttributes);
    if (a)
      return a;
  }
  return m_filteredModel->GetApi(componentAttributes);
}

RteApi* RteTarget::GetApi(const string& id) const
{
  RteProject* p = GetProject();
  const map<string, RteGpdscInfo*>& gpdscInfos = p->GetGpdscInfos();
  for (auto itg = gpdscInfos.begin(); itg != gpdscInfos.end(); itg++) {
    RteGpdscInfo* gi = itg->second;
    RteModel* m = gi->GetGeneratorModel();
    if (!m)
      continue;
    RteApi* a = m->GetApi(id);
    if (a)
      return a;
  }
  return m_filteredModel->GetApi(id);
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
  if (!c->IsGenerated() && inserted) {
    // check generated component
    if (inserted->IsGenerated()) {
      if (inserted->HasAttribute("generator") || c->HasAttribute("generator") || c->GetGpdscFile(this) == inserted->GetGpdscFile(this)) {
        string packPath = inserted->GetPackage()->GetAbsolutePackagePath();
        if (packPath != GetProject()->GetProjectPath()) // default gpdsc cannot be un-selected
          inserted->SetAttribute("selectable", "1"); // sets selecatble attribute to allow un-selection of a generated bootsrtrap component
          // for boot-strap components add doc file and description
        if (inserted->GetDescription().empty()) {
          inserted->SetText(c->GetDescription());
        } if (inserted->GetDocFile().empty()) {
          inserted->AddAttribute("doc", c->GetDocFile(), false);
        }
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
  if (inserted) {
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
  const map<string, RteGpdscInfo*>& gpdscInfos = p->GetGpdscInfos();
  for (auto itg = gpdscInfos.begin(); itg != gpdscInfos.end(); itg++) {
    RteGpdscInfo* gi = itg->second;
    if (!gi->IsUsedByTarget(GetName()))
      continue;
    RteModel* generatedModel = gi->GetGeneratorModel();
    if (generatedModel) {
      const RteComponentMap& componentList = generatedModel->GetComponentList();
      // fill unique filtered list
      RteComponentMap::const_iterator itc;
      for (itc = componentList.begin(); itc != componentList.end(); itc++) {
        RteComponent* c = itc->second;
        if (c->IsDeviceStartup())
          deviceStartup = c;
        AddFilteredComponent(c);
      }
    }
  }

  // fill unique filtered list from filtered model
  const RteComponentMap& componentList = m_filteredModel->GetComponentList();
  RteComponentMap::const_iterator itc;
  for (itc = componentList.begin(); itc != componentList.end(); itc++) {
    RteComponent* c = itc->second;
    if (deviceStartup && c->IsDeviceStartup())
      continue; // always take device startup from generated project
    ConditionResult r = c->Evaluate(GetFilterContext());
    if (r > FAILED) {
      AddFilteredComponent(c);
    }
  }
  // categorize component and filter files
  for (itc = m_filteredComponents.begin(); itc != m_filteredComponents.end(); itc++) {
    RteComponent* c = itc->second;
    RteApi* a = GetApi(c->GetAttributes());
    if (a && m_filteredApis.find(a->GetID()) == m_filteredApis.end()) { // component has an API and the API is not inserted yet
      m_filteredApis[a->GetID()] = a;
      CategorizeComponent(a);
    }
    CategorizeComponent(c);
  }

  // add potential components from global model
  RteModel* globalModel = GetModel();

  const RteComponentMap& allComponents = globalModel->GetComponentList();
  // fill unique filtered list
  for (itc = allComponents.begin(); itc != allComponents.end(); itc++) {
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
  for (auto itf = filteredFiles.begin(); itf != filteredFiles.end(); itf++) {
    RteFile* f = *itf;
    if (f && f->GetName() == name)
      return f;
  }
  return NULL;
}

RteFile* RteTarget::FindFile(const string& fileName, RteComponent* c) const
{
  const set<RteFile*>& filteredFiles = GetFilteredFiles(c);
  for (auto itf = filteredFiles.begin(); itf != filteredFiles.end(); itf++) {
    RteFile* f = *itf;
    if (f && RteUtils::ExtractFileName(f->GetName()) == fileName)
      return f;
  }
  return NULL;
}


RteFile* RteTarget::GetFile(const RteFileInstance* fi, RteComponent* c) const
{
  if (!fi)
    return NULL;
  const set<RteFile*>& filteredFiles = GetFilteredFiles(c);
  const string& deviceName = GetFullDeviceName();
  int index = fi->GetInstanceIndex();
  const string& instanceName = fi->GetInstanceName();
  for (auto itf = filteredFiles.begin(); itf != filteredFiles.end(); itf++) {
    RteFile* f = *itf;
    if (f && f->GetInstancePathName(deviceName, index) == instanceName) {
      return f;
    }
  }
  return NULL;
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
  for (auto itc = components.begin(); itc != components.end(); itc++) {
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
      const list<RteItem*>& files = fc->GetChildren();
      for (auto itf = files.begin(); itf != files.end(); itf++) {
        RteFile* f = dynamic_cast<RteFile*>(*itf);
        if (f && f->GetCategory() == RteFile::INCLUDE) {
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
  if (ci->IsApi())
    return m_filteredModel->GetApi(ci->GetComponentUniqueID(true));

  VersionCmp::MatchMode mode = ci->GetVersionMatchMode(GetName());
  RteComponent* c = NULL;
  if (mode == VersionCmp::FIXED_VERSION) {
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
      if (mode == VersionCmp::FIXED_VERSION)
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
    return GetModel()->GetApi(ci->GetComponentUniqueID(true));

  VersionCmp::MatchMode mode = ci->GetVersionMatchMode(GetName());
  RteComponent* c = NULL;
  if (mode == VersionCmp::FIXED_VERSION) {
    c = GetPotentialComponent(ci->GetComponentID(true));
  } else {
    c = GetLatestPotentialComponent(ci->GetComponentID(false));
  }
  return c;
}

RteItem::ConditionResult RteTarget::GetComponentsForApi(RteApi* api, set<RteComponent*>& components, bool selectedOnly) const
{
  ConditionResult result = MISSING;
  if (api) {
    // make copy of the attributes to remove api version
    map<string, string> apiAttributes = api->GetAttributes();
    if (selectedOnly) {
      auto it = apiAttributes.find("Capiversion");
      if (it != apiAttributes.end())
        apiAttributes.erase(it);
    }
    return GetComponentsForApi(api, apiAttributes, components, selectedOnly);
  }
  return result;
}


RteItem::ConditionResult RteTarget::GetComponentsForApi(RteApi* api, const map<string, string>& componentAttributes, set<RteComponent*>& components, bool selectedOnly) const
{
  bool exclusive = api != NULL && api->IsExclusive();
  ConditionResult result = MISSING;
  int nSelected = 0;
  for (auto it = m_filteredComponents.begin(); it != m_filteredComponents.end(); it++) {
    RteComponent* c = it->second;
    if (c->HasComponentAttributes(componentAttributes)) {
      if (IsComponentSelected(c)) {
        components.insert(c);
        nSelected++;
        if (exclusive && nSelected > 1)
          result = CONFLICT;
        else
          result = FULFILLED;
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

bool RteTarget::GenerateRteHeaders() {
  if (!GenerateRTEComponentsH())
    return false;

  string content;
  const set<string>& strings = GetGlobalPreIncludeStrings();
  for (auto s : strings) {
    content += s + carRet;
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

const bool RteTarget::GenerateRTEComponentsH() {

  string content;
  const string& devheader = GetDeviceHeader();
  if (!devheader.empty()) {            // found device header file.
    content += szDevHdr;
    content += "\"" + devheader + "\"" + carRet + carRet;
  }

  //---------------------------------------------------
  const set<string>& strings = GetRteComponentHstrings();
  for (auto s : strings) {
    content += s + carRet;
  }
  return GenerateRteHeaderFile("RTE_Components.h", content);
}

const bool RteTarget::GenerateRteHeaderFile(const string& headerName, const string& content) {

  RteProject *project = GetProject();
  if (!project)
    return false;

  string rteCompH = RteProject::GetRteHeader(headerName, GetName(), project->GetProjectPath());

  if (!RteFsUtils::MakeSureFilePath(rteCompH))
    return false;

  string HEADER_H;
  for (string::size_type pos = 0; pos < headerName.length(); pos++) {
    char ch = toupper(headerName[pos]);
    if (ch == '.')
      ch = '_';
    HEADER_H += ch;
  }

  // construct head comment

  ostringstream  oss;
  oss << szRteCh;
  oss << " * Project: '" << project->GetName() << "\' " << carRet;
  oss << " * Target:  '"  << GetName()         << "\' " << carRet;
  oss << " */" << carRet << carRet;

  // content
  oss << "#ifndef " << HEADER_H << carRet;
  oss << "#define " << HEADER_H << carRet << carRet;
  oss << content << carRet;
  oss << carRet;
  oss << "#endif /* " << HEADER_H << " */" << carRet;

  string str = oss.str();
  string fileBuf;

  // check if file has been changed
  RteFsUtils::ReadFile(rteCompH.c_str(), fileBuf);
  if (fileBuf.compare(str) == 0) {
    return true;
  }

  // file does not exist or its content is different
  RteFsUtils::CopyBufferToFile(rteCompH, str, false); // write file

  return true;
}
// End of RteTarget.cpp
