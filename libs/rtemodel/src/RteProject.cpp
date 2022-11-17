/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteProject.cpp
* @brief CMSIS RTE Instance in uVision Project
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteProject.h"

#include "RteComponent.h"
#include "RteFile.h"
#include "RteGenerator.h"
#include "RteModel.h"
#include "CprjFile.h"

#include "RteFsUtils.h"
#include "XMLTree.h"

using namespace std;

const std::string RteProject::DEFAULT_RTE_FOLDER = "RTE";

////////////////////////////
RteProject::RteProject() :
  RteItem(NULL),
  m_globalModel(NULL),
  m_callback(NULL),
  m_classes(NULL),
  m_nID(0),
  m_bInitialized(false),
  t_bGpdscListModified(false)
{
  m_packFilterInfos = new RteItemInstance(this);
  m_packFilterInfos->SetTag("filter");
  m_tag = "RTE";
}

RteProject::~RteProject()
{
  RteProject::Clear();
  delete m_packFilterInfos;
}

void RteProject::Clear()
{
  ClearClasses();
  ClearTargets();
  ClearMissingPacks();
  m_components.clear();
  m_projectPath.clear();
  m_files.clear();
  ClearFilteredPackages();

  for (auto it = m_gpdscInfos.begin(); it != m_gpdscInfos.end(); it++) {
    delete it->second;
  }
  m_gpdscInfos.clear();

  for (auto itb = m_boardInfos.begin(); itb != m_boardInfos.end(); itb++) {
    delete itb->second;
  }
  m_boardInfos.clear();

  m_filteredPackages.clear();
  RteItem::Clear();
  m_bInitialized = false;
  t_bGpdscListModified = false;
}

void RteProject::ClearMissingPacks()
{
  t_missingPackIds.clear();
  t_missingPackTargets.clear();
}

void RteProject::Initialize()
{
  ClearMissingPacks();
  RemoveGeneratedComponents();
  AddGeneratedComponents();
  Update();

  CollectMissingPacks();
  m_bInitialized = true;
}

void RteProject::SetModel(RteModel* model)
{
  m_globalModel = model;
}

 RteCallback* RteProject::GetCallback() const
 {
   return m_callback ? m_callback : RteCallback::GetGlobal();
 }

RteProject* RteProject::GetProject() const
{
  return const_cast<RteProject*>(this);
}

const string& RteProject::GetRteFolder() const {
  if (!m_rteFolder.empty()) {
    return m_rteFolder;
  }
  return DEFAULT_RTE_FOLDER;
}

const string& RteProject::GetRteFolder(const RteComponentInstance* ci) const {
  if (ci && !ci->GetRteFolder().empty()) {
    return ci->GetRteFolder();
  }
  return GetRteFolder();
}

void RteProject::ClearClasses()
{
  delete m_classes;
  m_classes = 0;
}

void RteProject::UpdateClasses()
{
  ClearClasses();
  m_classes = new RteComponentInstanceGroup(this);
  for (auto it = m_components.begin(); it != m_components.end(); it++)
  {
    RteComponentInstance* ci = it->second;
    CategorizeComponentInstance(ci);
  }
}

void RteProject::CategorizeComponentInstance(RteComponentInstance* ci)
{
  const string& className = ci->GetCclassName();
  const string& groupName = ci->GetCgroupName();
  const string& subName = ci->GetCsubName();
  RteComponentInstanceGroup* group = m_classes->EnsureGroup(className);

  if (!subName.empty() || ci->IsApi() || ci->GetApiInstance() || ci->HasAttribute("Capiversion")) {
    group = group->EnsureGroup(groupName);
  }
  group->AddComponentInstance(ci);
}

RteComponentInstanceGroup* RteProject::GetClassGroup(const string& className) const
{
  if (m_classes) {
    string corectedClassName;
    if (className.find("::") == 0)
      corectedClassName = className.substr(2);
    else
      corectedClassName = className;

    return m_classes->GetGroup(corectedClassName);
  }
  return NULL;
}

RteComponentInstanceAggregate* RteProject::GetComponentInstanceAggregate(RteComponentInstance* ci) const
{
  if (ci && m_classes) {
    const string& className = ci->GetCclassName();
    RteComponentInstanceGroup* classGroup = GetClassGroup(className);
    if (classGroup)
      return classGroup->GetComponentInstanceAggregate(ci);
  }
  return NULL;
}


RteComponentInstance* RteProject::GetComponentInstance(const string& id) const
{
  map<string, RteComponentInstance*>::const_iterator it = m_components.find(id);
  if (it != m_components.end())
    return it->second;
  return NULL;
}


RteComponentInstance* RteProject::GetApiInstance(const map<string, string>& componentAttributes) const
{
  for (auto itc = m_components.begin(); itc != m_components.end(); itc++) {
    RteComponentInstance* ci = itc->second;
    if (ci && ci->IsApi() && ci->MatchApiAttributes(componentAttributes))
      return ci;
  }
  return NULL;
}


RteFileInstance* RteProject::GetFileInstance(const string& id) const
{
  map<string, RteFileInstance*>::const_iterator it = m_files.find(id);
  if (it != m_files.end()) {
    return it->second;
  }
  // try to compare case insensitive
  for (it = m_files.begin(); it != m_files.end(); it++) {
    if ( RteUtils::EqualNoCase(id, it->first)) {
      return it->second;
    }
  }
  return NULL;
}

void RteProject::GetFileInstances(RteComponentInstance* ci, const string& targetName, map<string, RteFileInstance*>& configFiles) const
{
  for (auto itf = m_files.begin(); itf != m_files.end(); itf++) {
    RteFileInstance* fi = itf->second;
    if (!fi->IsUsedByTarget(targetName))
      continue;
    if (fi->GetComponentInstance(targetName) != ci)
      continue;
    const string& name = fi->GetName();
    configFiles[name] = fi;
  }
}


RteComponentInstance* RteProject::AddComponent(RteComponent* c, int instanceCount, RteTarget* target, RteComponentInstance* oldInstance)
{
  if (!c)
    return NULL;

  const string& id = c->GetID();
  string deviceName = target->GetFullDeviceName();

  RteComponentInstance* ci = GetComponentInstance(id);

  if (!ci) {
    ci = new RteComponentInstance(this);
    AddItem(ci);
    m_components[c->GetID()] = ci;
    ci->Init(c);
    // check if we have previous aggregate with Excluded flag for the target
  }
  // find previous flags (they can reside in a separate instance)
  if (!oldInstance && !c->IsApi() && m_classes) {
    RteComponentInstanceAggregate* ai = m_classes->GetComponentInstanceAggregate(c->GetComponentAggregateID());
    if (ai) {
      oldInstance = ai->GetComponentInstance(target->GetName());
    }
  }

  bool targetSpecific = false;
  if (oldInstance) {
    targetSpecific = oldInstance->IsTargetSpecific();
  }

  ci->SetRemoved(false);
  ci->SetTargetSpecific(targetSpecific);
  ci->SetResolvedComponent(c, target->GetName());
  RteInstanceTargetInfo* info = ci->AddTargetInfo(target->GetName());
  if (oldInstance && oldInstance != ci) {
    RteInstanceTargetInfo* oldInfo = oldInstance->GetTargetInfo(target->GetName());
    if (oldInfo) {
      info->CopySettings(*oldInfo);
    }
  }
  info->SetInstanceCount(instanceCount);

  if (instanceCount > 0) {
    // add gpdsc info if needed
    AddGpdscInfo(c, target);
  }

  return ci;
}

void RteProject::AddCprjComponents(const list<RteItem*>& selItems, RteTarget* target,
                                  set<RteComponentInstance*>& unresolvedComponents)
{
  map<string, string> configFileVersions;
  // first try to resolve the components
  for (auto its = selItems.begin(); its != selItems.end(); its++) {
    RteItem* item = *its;
    // Resolve component
    RteComponentInstance* ci = AddCprjComponent(item, target);
    RteComponent* component = ci->GetResolvedComponent(target->GetName());
    int instances = item->GetAttributeAsInt("instances", 1);
    if (component) {
      target->SelectComponent(component, instances, false, true);
      const list<RteItem*>& files = item->GetChildren();
      for (auto itf = files.begin(); itf != files.end(); itf++) {
        RteItem* f = *itf;
        if (f->GetTag() != "file")
          continue;
        const string& name = f->GetName();
        string version = f->GetAttribute("version");
        if (version.empty())
          version = item->GetAttribute("Cversion");
        if (version.empty())
          continue;
        configFileVersions[name] = version;
      }
    } else {
      target->SetComponentUsed(ci, instances);
      unresolvedComponents.insert(ci);
    }
  }
  Apply();

  // update file versions
  for (auto itf = m_files.begin(); itf != m_files.end(); itf++) {
    const string& instanceName = itf->first;
    RteFileInstance* fi = itf->second;
    string version;
    auto itver = configFileVersions.find(instanceName); // file in cprj can be specified by its instance name
    if (itver == configFileVersions.end())
      itver = configFileVersions.find(fi->GetName()); // or by original name
    if (itver != configFileVersions.end()) {
      version = itver->second;
    }
    UpdateFileInstanceVersion(fi, version);
    UpdateConfigFileBackups(fi, fi->GetFile(target->GetName()));
  }
}



RteComponentInstance* RteProject::AddComponent(const string& id)
{
  RteComponentInstance* ci = new RteComponentInstance(this);
  AddItem(ci);
  m_components[id] = ci;
  return ci;
}



RteComponentInstance* RteProject::AddCprjComponent(RteItem* item, RteTarget* target) // from cprj
{
  RteComponentInstance* ci = new RteComponentInstance(this);
  AddItem(ci);
  ci->InitInstance(item);
  int instanceCount = item->GetAttributeAsInt("instances", 1);
  ci->RemoveAttribute("instances"); // goes to target info
  RteInstanceTargetInfo* info = ci->AddTargetInfo(target->GetName());
  info->SetInstanceCount(instanceCount);
  const string& version = item->GetAttribute("Cversion");
  if (version.empty()) {
    info->SetVersionMatchMode(VersionCmp::MatchMode::LATEST_VERSION);
  } else {
    info->SetVersionMatchMode(VersionCmp::MatchMode::FIXED_VERSION);
  }
  if (item->GetAttribute("instances").empty())
    instanceCount = -1;
  RteComponent* component = target->ResolveComponent(ci);
  string id;
  if (component){
    ci->Init(component);
    ci->SetResolvedComponent(component, target->GetName());
    id = component->GetID();
  } else {
    id = ci->ConstructID();
  }
  string layer = item->GetAttribute("layer");
  if (!layer.empty())
    ci->AddAttribute("layer", layer);
  string rtePath = item->GetAttribute("rtedir");
  if (!rtePath.empty())
    ci->AddAttribute("rtedir", rtePath);
  m_components[id] = ci;
  return ci;
}


bool RteProject::RemoveComponent(const string& id)
{
  map<string, RteComponentInstance*>::iterator it = m_components.find(id);
  if (it != m_components.end()) {
    RteComponentInstance* ci = it->second;
    m_components.erase(it);
    RemoveItem(ci);
    delete ci;
    return true;
  }
  return false;
}


void RteProject::AddComponentFiles(RteComponentInstance* ci, RteTarget* target, set<RteFile*>& forcedFiles)
{
  const string& targetName = target->GetName();
  RteComponent* c = ci->GetResolvedComponent(targetName);
  if (!c)
    return; // not resolved => nothing to add
  int instanceCount = ci->GetInstanceCount(targetName);
  bool excluded = ci->IsExcluded(targetName);
  // add files
  const set<RteFile*>& files = target->GetFilteredFiles(c);
  for (auto itf = files.begin(); itf != files.end(); itf++) {
    RteFile* f = *itf;
    if (!f)
      continue;
    if (f->IsConfig()) {
      for (int i = 0; i < instanceCount; i++) {
        RteFileInstance* fi = AddFileInstance(ci, f, i, target);
        fi->SetExcluded(excluded, targetName);
      }
    } else if (f->IsForcedCopy()) {
      forcedFiles.insert(f);
    } else {
      target->AddComponentInstanceForFile(f->GetOriginalAbsolutePath(), ci);
    }
  }
}


RteFileInstance* RteProject::AddFileInstance(RteComponentInstance* ci, RteFile* f, int index, RteTarget* target)
{
  if (!f || !f->IsConfig())
    return NULL;

  string deviceName = target->GetFullDeviceName();
  string id = f->GetInstancePathName(deviceName, index, GetRteFolder(ci));
  target->AddComponentInstanceForFile(id, ci);

  string savedVersion = "0.0.0"; // unknown version

  RteFileInstance* fi = GetFileInstance(id);
  if (fi) {
    savedVersion = fi->GetVersionString();
  } else {
    fi = new RteFileInstance(this);
    AddItem(fi);
    m_files[id] = fi;
  }
  InitFileInstance(fi, f, index, target, savedVersion, GetRteFolder(ci));
  return fi;
}

bool RteProject::UpdateFileToNewVersion(RteFileInstance* fi, RteFile* f, bool bMerge)
{
  if (!fi || !f) {
    return false;
  }
  if (UpdateFileInstance(fi, f, bMerge, true)) {
    UpdateConfigFileBackups(fi, f);
    return true;
  }
  return false;
}

void RteProject::InitFileInstance(RteFileInstance* fi, RteFile* f, int index, RteTarget* target, const string& savedVersion, const string& rteFolder)
{
  string deviceName = target->GetFullDeviceName();
  const string& targetName = target->GetName();

  fi->Init(f, deviceName, index, rteFolder);
  fi->Update(f, false);
  fi->AddTargetInfo(targetName); // set/update supported targets
  fi->SetRemoved(false);
  string absPath = fi->GetAbsolutePath();
  bool bExists = RteFsUtils::Exists(absPath);
  if (!bExists) {
    UpdateFileInstance(fi, f, false, false);
  } else {
    UpdateFileInstanceVersion(fi, savedVersion);
  }
  UpdateConfigFileBackups(fi, f);
}

void RteProject::WriteInstanceFiles(const std::string& targetName)
{
  for (auto it = m_files.begin(); it != m_files.end(); ++it) {
    const string& fileID = it->first;
    RteFileInstance* fi = it->second;
    RteFile* f = fi->GetFile(targetName);

    if (fi->IsRemoved() || fi->GetTargetCount() == 0) {
      RemoveFileInstance(fileID);
    } else {
      if (!RteFsUtils::Exists(fi->GetAbsolutePath())) {
        UpdateFileInstance(fi, f, false, false);
      }
      UpdateConfigFileBackups(fi, f);
    }
  }
}
bool RteProject::UpdateFileInstance(RteFileInstance* fi, RteFile* f, bool bMerge, bool bUpdateComponent)
{
  if (!ShouldUpdateRte())
    return true;

  while (!fi->Copy(f, bMerge)) {
    string str = "Error: cannot copy file\n";
    str += f->GetOriginalAbsolutePath();
    str += "\n to\n";
    str += fi->GetAbsolutePath();
    str += "\nOperation failed\n";
    long res = GetCallback()->QueryMessage(str, RTE_MB_RETRYCANCEL | RTE_MB_ICONEXCLAMATION, RTE_IDCANCEL);
    if (res == RTE_IDCANCEL) {
      return false;
    } else if (res != RTE_IDRETRY) {
      break;
    }
  }
  fi->Update(f, bUpdateComponent); // for an existing file, update its origin

  return true;
}

void RteProject::UpdateFileInstanceVersion(RteFileInstance* fi, const string& savedVersion) {
  string baseVersion;
  // try to find version from the base file if exists
  string absPath = RteFsUtils::AbsolutePath(fi->GetAbsolutePath()).generic_string();
  string dir = RteUtils::ExtractFilePath(absPath, false);
  string name = RteUtils::ExtractFileName(absPath);
  string baseName = name + '.' + RteUtils::BASE_STRING;
  list<string> backupFileNames;
  RteFsUtils::GrepFileNames(backupFileNames, dir, baseName + "@*");
  if (!backupFileNames.empty()) {
    // sort by version descending
    backupFileNames.sort([](const string& s0, const string& s1) {
      string v0 = RteUtils::GetSuffix(s0, '@');
      string v1 = RteUtils::GetSuffix(s1, '@');
      return VersionCmp::Compare(v0, v1) > 0;
      });
    baseVersion = RteUtils::GetSuffix(*backupFileNames.begin(), '@'); // use the top version
  }
  if (baseVersion.empty()) {
    baseVersion = savedVersion;
  }
  fi->AddAttribute("version", baseVersion, false);
}


void RteProject::UpdateConfigFileBackups(RteFileInstance* fi, RteFile* f)
{
  if (!ShouldUpdateRte())
    return;

  string src = f->GetOriginalAbsolutePath();
  string absPath = RteFsUtils::AbsolutePath(fi->GetAbsolutePath()).generic_string();
  string dir = RteUtils::ExtractFilePath(absPath, false);
  string name = RteUtils::ExtractFileName(absPath);
  const string& baseVersion = fi->GetVersionString();
  const string& updateVersion = f->GetVersionString();
  string baseFile = RteUtils::AppendFileBaseVersion(absPath, baseVersion);
  if (!RteFsUtils::Exists(baseFile)) {
    // create base file if possible
    if (baseVersion == updateVersion) {
      // we can use current version as base one
      RteFsUtils::CopyCheckFile(src, baseFile, false);
      RteFsUtils::SetFileReadOnly(baseFile, true);
    } else {
      baseFile.clear(); //no such file
    }
  }
  // copy current file if version differs
  string updateFile = RteUtils::AppendFileUpdateVersion(absPath, updateVersion);
  if (baseVersion != updateVersion) {
    RteFsUtils::CopyCheckFile(src, updateFile, false);
    RteFsUtils::SetFileReadOnly(updateFile, true);
  } else {
    updateFile.clear(); // no need in that
  }

  // remove redundant current and origin files
  string baseName = name + '.' + RteUtils::BASE_STRING;
  string updateName = name + '.' + RteUtils::UPDATE_STRING;

  list<string> backupFileNames;
  RteFsUtils::GrepFileNames(backupFileNames, dir, baseName + "@*");
  RteFsUtils::GrepFileNames(backupFileNames, dir, updateName + "@*");
  for (string fileName : backupFileNames) {
    error_code ec;
    if (!fs::equivalent(fileName, baseFile, ec) && !fs::equivalent(fileName, updateFile, ec)) {
      RteFsUtils::DeleteFileAutoRetry(fileName);
    }
  }
}


void RteProject::MergeFiles(const string& curFile, const string& updateFile, const string& baseFile)
{
  if (!baseFile.empty() && RteFsUtils::Exists(baseFile)) {
    GetCallback()->MergeFiles3Way(curFile, updateFile, baseFile);
  } else {
    GetCallback()->MergeFiles(curFile, updateFile);
  }
}


bool RteProject::RemoveFileInstance(const string& id)
{
  if (!ShouldUpdateRte())
    return true;

  map<string, RteFileInstance*>::iterator it = m_files.find(id);
  if (it != m_files.end()) {
    RteFileInstance* fi = it->second;
    if (fi->IsConfig()) {
      fi->SetRemoved(true); // for config files-set removed, do not delete
    } else {
      RemoveItem(fi);
      m_files.erase(it);
      delete fi;
      return true;
    }
  }
  return false;
}

void RteProject::DeleteFileInstance(RteFileInstance* fi) {
  if (!fi)
    return;

  map<string, RteFileInstance*>::iterator it = m_files.find(fi->GetID());
  if (it != m_files.end()) {
    m_files.erase(it);
  }
  RemoveItem(fi);
  delete fi;

}


void RteProject::AddGeneratedComponents()
{
  for (auto itg = m_gpdscInfos.begin(); itg != m_gpdscInfos.end(); itg++) {
    RteGpdscInfo* gi = itg->second;
    RteModel* m = gi->GetGeneratorModel();
    if (!m)
      continue;
    for (auto itt = m_targets.begin(); itt != m_targets.end(); itt++) {
      const string& targetName = itt->first;
      if (!gi->IsUsedByTarget(targetName))
        continue;
      RteTarget* target = itt->second;
      const RteComponentMap& components = m->GetComponentList();
      for (auto itc = components.begin(); itc != components.end(); itc++) {
        RteComponent* c = itc->second;
        AddComponent(c, 1, target, NULL);
      }
    }
  }
}

void RteProject::RemoveGeneratedComponents() {
  // mark all generated components as removed (flag will be reset later, but only for those that remain in the list)
  for (auto it = m_components.begin(); it != m_components.end(); it++)
  {
    RteComponentInstance* ci = it->second;
    if (ci->IsGenerated())
      ci->SetRemoved(true);
  }
}


bool RteProject::Apply()
{
  t_bGpdscListModified = false;
  if (!m_globalModel)
    return false;

  // mark all components as removed (flag will be reset later, but only for those that remain in the list)
  for (auto it = m_components.begin(); it != m_components.end(); it++)
  {
    RteComponentInstance* ci = it->second;
    ci->SetRemoved(true);
  }

  // add/update components
  for (auto itt = m_targets.begin(); itt != m_targets.end(); itt++) {
    const string& targetName = itt->first;
    RteTarget* target = itt->second;

    //// check for removed generated components
    set<RteComponentAggregate*> unselectedGpdscCompoinents;
    target->GetUnSelectedGpdscAggregates(unselectedGpdscCompoinents);
    for (auto itu = unselectedGpdscCompoinents.begin(); itu != unselectedGpdscCompoinents.end(); itu++) {
      RteComponentAggregate* ua = *itu;
      RteComponent* c = ua->GetComponent();
      if (!c)
        continue;
      const string& gpdsc = c->GetGpdscFile(target);
      if (gpdsc.empty())
        continue;
      RteGpdscInfo* gpdscInfo = GetGpdscInfo(gpdsc);
      if (!gpdscInfo)
        continue;
      gpdscInfo->RemoveTargetInfo(targetName);
    }

    const map<RteComponentAggregate*, int>& components = target->CollectSelectedComponentAggregates();
    for (auto itc = components.begin(); itc != components.end(); itc++) {
      RteComponentAggregate* a = itc->first;
      int count = itc->second;
      RteComponent* c = a->GetComponent();
      RteComponentInstance* ci = a->GetComponentInstance();

      if (c) {
        if (ci && c->IsGenerated()) {
          const string& gpdsc = c->GetPackage()->GetPackageFileName();
          RteGpdscInfo* gpdscInfo = GetGpdscInfo(gpdsc);
          if (!gpdscInfo || !gpdscInfo->IsUsedByTarget(targetName)) {
            ci->SetRemoved(true); // gpdsc is removed => all files are removed
            continue;
          }
        }
        ci = AddComponent(c, count, target, ci);
        // add API if any
        RteApi* api = c->GetApi(target, true);
        if (api) {
          ci = AddComponent(api, 1, target, ci);
        }
      } else if (ci) {
        // keep unresolved components and their APIs in the project
        ci->SetRemoved(false);
        RteInstanceTargetInfo* ti = ci->AddTargetInfo(targetName);
        ti->SetInstanceCount(count);

        RteComponentInstance* apiInstance = ci->GetApiInstance();
        if (apiInstance) {
          apiInstance->SetRemoved(false);
          ti = apiInstance->AddTargetInfo(targetName);
          ti->SetInstanceCount(1);
        }
      }
    }
    // check if gpdsc infos are actual
    for (auto itgi = m_gpdscInfos.begin(); itgi != m_gpdscInfos.end(); itgi++) {
      if (!target->IsGpdscUsed(itgi->first)) {
        RteGpdscInfo* gpdscInfo = itgi->second;
        gpdscInfo->RemoveTargetInfo(targetName);
      }
    }
  }

  Update();
  return t_bGpdscListModified;
}


bool RteProject::ApplyInstanceChanges()
{
  if (!m_globalModel || !m_classes)
    return false;
  // go through changed components
  set<RteComponentInstanceAggregate*> modifiedAggregates;
  m_classes->GetModifiedInstanceAggregates(modifiedAggregates);
  if (modifiedAggregates.empty())
    return false;

  RteTarget* activeTarget = GetActiveTarget();
  const string& activeTargetName = activeTarget->GetName();
  for (auto it = modifiedAggregates.begin(); it != modifiedAggregates.end(); it++) {
    RteComponentInstanceAggregate* a = *it;

    RteComponentInstance* orig = a->GetModifiedInstance();
    if (!orig)
      continue;
    RteComponentInstance* copy = orig->GetCopy(); // we actually need a copy that contains new values
    if (!copy)
      continue;
    RteInstanceTargetInfo* ti = copy->GetTargetInfo(activeTargetName);
    if (!ti)
      continue;

    // we have here several situations
    if (copy->IsRemoved()) {
      // item is removed => remove it for all targets
      const list<RteItem*>& children = a->GetChildren();
      for (auto itc = children.begin(); itc != children.end(); itc++) {
        RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(*itc);
        if (ci) {
          ci->SetRemoved(true);
        }
      }
      continue;
    }

    // if target excluded :
    if (copy->IsExcluded(activeTargetName)) {
      // only change excluded flag for current target in the original component instance
      // do nothing else
      orig->SetExcluded(true, activeTargetName);
      orig->CopyTargetSettings(*ti, activeTargetName);
      continue;
    }
    bool targetSpecific = copy->IsTargetSpecific();
    int instanceCount = copy->GetInstanceCount(activeTargetName);

    // ensure we have an instance for active target (in case of if variant/version change it can be missing)
    RteComponent* c = copy->ResolveComponent(activeTargetName); // component should be resolvable, otherwise no changes
    if (!c) {
      orig->SetExcluded(false, activeTargetName);
      continue;
    }
    RteComponentInstance* ciNew = AddComponent(c, instanceCount, activeTarget, copy);

    const list<RteItem*>& children = a->GetChildren();
    for (auto itc = children.begin(); itc != children.end(); itc++) {
      RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(*itc);
      if (!ci)
        continue;
      if (targetSpecific) {
        // remove target from other instances in the aggregate
        if (ci != ciNew) {
          ci->RemoveTargetInfo(activeTargetName);
          ci->SetTargetSpecific(true);
        }
      } else { // apply to all targets
      // remove all other components
        const RteInstanceTargetInfoMap& infos = ci->GetTargetInfos();
        for (auto iti = infos.begin(); iti != infos.end(); iti++) {
          const string& targetName = iti->first;
          if (targetName != activeTargetName) {
            RteInstanceTargetInfo* info = iti->second;
            RteTarget* t = GetTarget(targetName);
            // will effectively add target info and files for new target
            AddComponent(c, copy->GetInstanceCount(activeTargetName), t, copy);
            ciNew->CopyTargetSettings(*ti, targetName);
            ciNew->SetExcluded(info->IsExcluded(), targetName); // preserve excluded flag
          }
        }
        if (ci != ciNew) {
          ci->SetRemoved(true);
          ci->ClearTargets();
        }
      }
    }
  }
  Update(); // true: query to update config files
  return true;
}


void RteProject::Update()
{
  ClearClasses();

  CollectFilteredPackagesFromTargets();
  ResolvePacks();

  // remove components that no longer used by any target
  for (auto itc = m_components.begin(); itc != m_components.end(); )
  {
    auto itcurrent = itc++;
    RteComponentInstance* ci = itcurrent->second;
    // remove targets with instance count == 0
    ci->PurgeTargets();
    if (ci->IsRemoved()) {
      RemoveComponent(itcurrent->first);
    }
  }
  // remove gpdsc infos that no longer used by any target
  bool gpdscRemoved = false;
  for (auto itg = m_gpdscInfos.begin(); itg != m_gpdscInfos.end(); ) {
    auto itgcur = itg++;
    RteGpdscInfo* gi = itgcur->second;
    // remove targets with instance count == 0
    gi->PurgeTargets();
    if (gi->IsRemoved() || gi->GetTargetCount() == 0) {
      m_gpdscInfos.erase(itgcur);
      delete gi;
      gpdscRemoved = true;
    }
  }

  if (gpdscRemoved) {
    t_bGpdscListModified = true;
    FilterComponents();
  }

  // remove board infos that no longer used by any target and resolve boards otherwise
  for (auto itb = m_boardInfos.begin(); itb != m_boardInfos.end(); ) {
    auto itbcur = itb++;
    RteBoardInfo* bi = itbcur->second;
    // remove targets with instance count == 0
    bi->PurgeTargets();
    if (bi->IsRemoved() || bi->GetTargetCount() == 0) {
      m_boardInfos.erase(itbcur);
      delete bi;
    } else {
      bi->ResolveBoard();
    }
  }


  UpdateClasses();
  ResolveComponents(false);
  UpdateModel();

  // update/remove files
  map<RteFileInstance*, RteFile*>::iterator itfu;
  for (auto itf = m_files.begin(); itf != m_files.end();)
  {
    auto itcurrent = itf++;
    RteFileInstance* fi = itcurrent->second;
    if (!fi->IsRemoved()) {
      int instanceIndex = fi->GetInstanceIndex();
      string aggregateID = fi->GetComponentAggregateID();
      RteComponentInstanceAggregate* ai = m_classes->GetComponentInstanceAggregate(aggregateID);
      if (ai) {
        for (auto itt = m_targets.begin(); itt != m_targets.end(); itt++) {
          RteTarget* target = itt->second;
          const string& targetName = itt->first;
          if (fi->IsFilteredByTarget(targetName)) {
            RteComponentInstance* ci = ai->GetComponentInstance(targetName);
            if (ci && ci->IsFilteredByTarget(targetName) && instanceIndex < ci->GetInstanceCount(targetName)) {
              bool excluded = ci->IsExcluded(targetName);
              fi->SetExcluded(excluded, targetName);
              RteComponent* c = ci->GetResolvedComponent(targetName);
              if (!c) // missing component?
                continue; // leave available until missing is resolved

                // check if component has the file for given target (resolved component can have another set of files, even config ones)
              RteFile *f = target->GetFile(fi, c, GetRteFolder());
              if (f) {
                continue;
              }
            }
          }
          // file does not belong to any component in the target or not filtered by target
          fi->RemoveTargetInfo(targetName);
        }
      } else {
        fi->ClearTargets();
      }
    }
    if (fi->IsRemoved() || fi->GetTargetCount() == 0)
      RemoveFileInstance(itcurrent->first);
  }


  // add files
  set<RteFile*> forcedFiles;
  for (auto itt = m_targets.begin(); itt != m_targets.end(); itt++) {
    RteTarget* target = itt->second;
    for (auto itc = m_components.begin(); itc != m_components.end(); itc++) {
      RteComponentInstance* ci = itc->second;
      AddComponentFiles(ci, target, forcedFiles);
    }
  }

  // add forced copy files
  for (auto itf = forcedFiles.begin(); itf != forcedFiles.end(); itf++) {
    RteFile* f = *itf;
    string dst = GetProjectPath() + f->GetInstancePathName(EMPTY_STRING, 0, GetRteFolder());

    error_code ec;
    if (fs::exists(dst, ec))
      continue;
    string src = f->GetOriginalAbsolutePath();
    if (!RteFsUtils::CopyCheckFile(src, dst, false)) {
      string str = "Error: cannot copy file\n";
      str += src;
      str += "\n to\n";
      str += dst;
      str += "\nOperation failed\n";
      GetCallback()->OutputErrMessage(str);
    }
  }

  CollectSettings();
}

void RteProject::GenerateRteHeaders() {
  if (!ShouldUpdateRte())
    return;

  // generate header files for all targets
  for (auto itt : m_targets) {
    RteTarget* target = itt.second;
    if (target) {
      target->GenerateRteHeaders();
    }
  }
}

void RteProject::ResolvePacks()
{
  for (auto itpi = m_filteredPackages.begin(); itpi != m_filteredPackages.end(); itpi++) {
    RtePackageInstanceInfo* pi = itpi->second;
    pi->ResolvePack();
  }
}


RtePackageInstanceInfo* RteProject::GetPackageInfo(const string& packId) const
{
  auto it = m_filteredPackages.find(packId);
  if (it != m_filteredPackages.end())
    return it->second;
  string commonId = RtePackage::CommonIdFromId(packId);
  if (packId == commonId)
    return GetLatestPackageInfo(commonId);

  return NULL;
}

RtePackageInstanceInfo* RteProject::GetLatestPackageInfo(const string& packId) const
{
  RtePackageInstanceInfo* latestPi = NULL;
  string commonId = RtePackage::CommonIdFromId(packId);
  for (auto it = m_filteredPackages.begin(); it != m_filteredPackages.end(); it++) {
    if (commonId == RtePackage::CommonIdFromId(it->first)) {
      string version = RtePackage::VersionFromId(it->first);
      RtePackageInstanceInfo* pi = it->second;
      if (!latestPi || VersionCmp::Compare(latestPi->GetVersionString(), version) < 0)
        latestPi = pi;
    }
  }
  return latestPi;
}

string RteProject::GetEffectivePackageID(const string& packId, const string& targetName) const
{
  RtePackageInstanceInfo* pi = GetPackageInfo(packId);
  if (pi && pi->GetVersionMatchMode(targetName) == VersionCmp::MatchMode::FIXED_VERSION) {
    return pi->GetPackageID(true);
  }
  string commonId = RtePackage::CommonIdFromId(packId);
  string version = RtePackage::VersionFromId(packId);
  if (version.empty())
    return commonId;
  // check if latest available pack is older than required one
  RteModel* model = GetModel();
  RtePackage* latestPack = model->GetLatestPackage(commonId);
  if (latestPack) {
    if (VersionCmp::Compare(version, latestPack->GetVersionString()) > 0) {
      return RtePackage::ReleaseIdFromId(packId);
    }
  }
  return commonId;
}

RteGpdscInfo* RteProject::AddGpdscInfo(const string& gpdscFile, RteGeneratorModel* model)
{
  string name = gpdscFile;
  if (!m_projectPath.empty() && name.find(m_projectPath) == 0) {
    name = name.substr(m_projectPath.size());
  }

  RteGpdscInfo* gi = GetGpdscInfo(gpdscFile);
  if (!gi) {
    gi = new RteGpdscInfo(this, model);
    gi->AddAttribute("name", name);
    m_gpdscInfos[gpdscFile] = gi;
    t_bGpdscListModified = true;
  } else {
    gi->SetGeneratorModel(model);
  }

  return gi;
}


RteGpdscInfo* RteProject::AddGpdscInfo(RteComponent* c, RteTarget* target)
{
  if (!c || c->IsGenerated()) // it is already generated => gpdsc info is present
    return NULL;

  RteGenerator* gen = c->GetGenerator();
  if (!gen)
    return NULL; // nothing to insert


    // get absolute gpdsc file name
  string gpdsc = c->GetGpdscFile(target);
  if (gpdsc.empty())
    return NULL;

  RteGpdscInfo* gi = GetGpdscInfo(gpdsc);
  if (!gi) {
    gi = AddGpdscInfo(gpdsc, 0);
    error_code ec;
    if (ShouldUpdateRte() && !fs::exists(gpdsc, ec)) { // file not exists
    // create destination directory
      string dir = RteUtils::ExtractFilePath(gpdsc, true);
      // we need the to directory to exist for modification watch
      while (true) {
        fs::create_directories(dir, ec);
        if (fs::exists(dir, ec)) {
          break;
        }
        // Display the error message and exit the process
        string str = "Error: cannot create directory: ";
        str += dir;
        str += "\n";
        long res = GetCallback()->QueryMessage(str, RTE_MB_RETRYCANCEL | RTE_MB_ICONEXCLAMATION, RTE_IDCANCEL);
        if (res != RTE_IDRETRY) {
          break;
        }
      }
    }
  }
  if (gi) {
    gi->AddTargetInfo(target->GetName());
  }
  return gi;
}


RteGpdscInfo* RteProject::GetGpdscInfo(const string& gpdscFile)
{
  auto it = m_gpdscInfos.find(gpdscFile);
  if (it != m_gpdscInfos.end())
    return it->second;
  return NULL;
}

bool RteProject::HasGpdscModels() const
{
  return !m_gpdscInfos.empty();
}

bool RteProject::HasMissingGpdscModels() const
{
  if (m_gpdscInfos.empty())
    return false;
  for (auto itg = m_gpdscInfos.begin(); itg != m_gpdscInfos.end(); itg++) {
    RteGpdscInfo* gi = itg->second;
    RteModel* m = gi->GetGeneratorModel();
    if (!m)
      return true;
  }
  return false;
}

RteGeneratorModel* RteProject::GetGpdscModelForTaxonomy(const string& taxonomyID)
{
  for (auto itg = m_gpdscInfos.begin(); itg != m_gpdscInfos.end(); itg++) {
    RteGpdscInfo* gi = itg->second;
    RteGeneratorModel* m = gi->GetGeneratorModel();
    if (m) {
      RteItem* taxonomy = m->GetTaxonomyItem(taxonomyID);
      if (taxonomy)
        return m;
    }
  }
  return NULL;
}

RteBoardInfo* RteProject::GetBoardInfo(const string& boardId) const
{
  auto it = m_boardInfos.find(boardId);
  if (it != m_boardInfos.end())
    return it->second;
  return nullptr;
}


RteBoardInfo* RteProject::GetTargetBoardInfo(const string& targetName) const
{
  for (auto itb = m_boardInfos.begin(); itb != m_boardInfos.end(); itb++) {
    RteBoardInfo* bi = itb->second;
    if (bi->IsUsedByTarget(targetName)) {
      return bi;
    }
  }
  return nullptr;
}

RteBoardInfo* RteProject::SetBoardInfo(const string& targetName, RteBoard* board)
{
  RteBoardInfo* bi = GetTargetBoardInfo(targetName);
  if (bi) {
    if (bi->GetBoard() == board) {
      return bi;
    }
    bi->RemoveTargetInfo(targetName);
  }
  if (!board)
    return nullptr;

  string id = board->GetDisplayName();
  bi = GetBoardInfo(id);
  if (!bi) {
    bi = new RteBoardInfo(this);
    bi->Init(board);
    m_boardInfos[id] = bi;
  }
  bi->AddTargetInfo(targetName);
  return bi;
}

RteBoardInfo* RteProject::CreateBoardInfo(RteTarget* target, CprjTargetElement* createTarget)
{
  if (!target || !createTarget || !createTarget->HasAttribute("Bname"))
    return nullptr;
  const string& targetName = target->GetName();
  RteBoardInfo* bi = new RteBoardInfo(this);
  bi->InitInstance(createTarget);

  string id = bi->GetDisplayName();
  m_boardInfos[id] = bi;
  bi->Init(target->FindBoard(bi->GetDisplayName()));
  bi->AddTargetInfo(targetName);
  return bi;
}

void RteProject::CollectSettings()
{
  for (auto it = m_targets.begin(); it != m_targets.end(); it++) {
    CollectSettings(it->first);
  }
}


string RteProject::GetRteComponentsH(const string & targetName, const string & prefix) const
{
  return GetRteHeader(string("RTE_Components.h"), targetName, prefix);
}

string RteProject::GetRteHeader(const string& name, const string & targetName, const string & prefix) const
{
  string rteHeader = prefix;
  rteHeader += GetRteFolder() + "/_";
  rteHeader += WildCards::ToX(targetName);
  rteHeader += "/";
  rteHeader += name;
  return rteHeader;
}


void RteProject::CollectSettings(const string& targetName)
{
  if (!m_globalModel)
    return;
  RteTarget* t = GetTarget(targetName);
  if (!t) {
    return;
  }
  t->ClearCollections();

  // collect includes, libs and RTE_Components_h defines for active target
  for (auto itc = m_components.begin(); itc != m_components.end(); itc++) {
    RteComponentInstance* ci = itc->second;
    t->CollectComponentSettings(ci);
  }
  t->CollectClassDocs();

  // collect copied files and sources
  for (auto itf = m_files.begin(); itf != m_files.end(); itf++)
  {
    RteFileInstance* fi = itf->second;
    t->AddFileInstance(fi);
  }

  // add generator headers and include paths

  for (auto itg = m_gpdscInfos.begin(); itg != m_gpdscInfos.end(); itg++) {
    RteGpdscInfo* gi = itg->second;
    RteGeneratorModel* m = gi->GetGeneratorModel();
    if (!m)
      continue;
    RteGenerator* gen = m->GetGenerator();
    if (!gen)
      continue;
    RteFileContainer* projectFiles = gen->GetProjectFiles();
    if (projectFiles) {
      string comment = gen->GetName();
      comment += ":Common Sources";

      const list<RteItem*>& children = projectFiles->GetChildren();
      for (auto itf = children.begin(); itf != children.end(); itf++) {
        RteFile* f = dynamic_cast<RteFile*>(*itf);
        if (!f)
          continue;
        RteFile::Category cat = f->GetCategory();
        switch (cat) {
        case RteFile::HEADER:
          t->AddFile(f->GetIncludeFileName(), cat, comment);
          // intended fall through
        case RteFile::INCLUDE:
          t->AddIncludePath(f->GetIncludePath());
        default:
          break;
        }
      }
    }
  }

  // check if RTE components are used before setting RTE_Components.h include path and adding to target as well as setting -D_RTE_ at the command line.
  // add .\RTE\_TargetName\RTE_Components.h filePath
  if (GetComponentCount() > 0) {
    string rteComponentsH = GetRteComponentsH(targetName, "./");
    t->AddIncludePath(RteUtils::ExtractFilePath(rteComponentsH, false));
    t->AddFile("RTE_Components.h", RteFile::HEADER, "Component selection"); // add ".\RTE\_TargetName\RTE_Components.h" folder to all target includes
    t->InsertDefine("_RTE_");
  }
  // add device properties
  const string& processorName = t->GetProcessorName();
  RteDeviceItem* d = t->GetDevice();
  if (!d) {
    // ensure we get the device (if found in packs)
    string vendor = t->GetVendorName();
    string fullDeviceName = t->GetFullDeviceName();
    d = m_globalModel->GetDevice(fullDeviceName, vendor);
  }
  t->AddDeviceProperties(d, processorName);
}

RteItem::ConditionResult RteProject::ResolveComponents(bool bFindReplacementForActiveTarget)
{
  ConditionResult res = FULFILLED;
  RteTarget* t = GetActiveTarget();
  if (!t)
    return res;
  const string& activeTargetName = t->GetName();
  for (auto itc = m_components.begin(); itc != m_components.end(); itc++) {
    RteComponentInstance* ci = itc->second;
    ci->ResolveComponent();
    if (!bFindReplacementForActiveTarget || ci->IsApi())
      continue;
    if (!ci->IsUsedByTarget(activeTargetName))
      continue;

    RteComponent* c = ci->GetResolvedComponent(activeTargetName);
    if (c)
      continue; // already resolved
    RteComponentAggregate* a = NULL;
    set<RteComponentAggregate*> aggregates;
    RteAttributes componentAttributes = *ci; // copy attributes
    if (ci->GetVersionMatchMode(activeTargetName) != VersionCmp::MatchMode::FIXED_VERSION)
    {
      // make search wider : remove bundle and version
      componentAttributes.RemoveAttribute("Cbundle");
      componentAttributes.RemoveAttribute("Cversion");
      componentAttributes.RemoveAttribute("condition");
      if (ci->GetCbundleName() == ci->GetCgroupName()) {
        componentAttributes.RemoveAttribute("Cgroup"); // this is to support old BoardSupport bundles where group name is the same as bundle
      }

      // first try to find a component from the same vendor
      t->GetComponentAggregates(componentAttributes.GetAttributes(), aggregates);

      // try to find a component from any vendor
      if (aggregates.empty()) {
        componentAttributes.RemoveAttribute("Cvendor");
        t->GetComponentAggregates(componentAttributes.GetAttributes(), aggregates);
      }

      // try to find a component with any variant
      if (aggregates.empty() && !componentAttributes.GetCvariantName().empty()) {
        componentAttributes.RemoveAttribute("Cvariant");
        t->GetComponentAggregates(componentAttributes.GetAttributes(), aggregates);
      }

    }
    ConditionResult r = FULFILLED;
    if (aggregates.size() == 1) {
      a = *aggregates.begin(); // only if one match exist
      const string& variant = ci->GetCvariantName();
      c = a->GetLatestComponent(variant);
      if (!c)
        c = a->GetComponent();
      ci->SetResolvedComponent(c, activeTargetName);
    } else if (aggregates.empty()) {
      r = ci->GetResolveResult(activeTargetName);
    } else {
      r = INSTALLED;
    }
    if (r < res)
      res = r;
  }
  return res;
}

/////////////////////////////
// Targets
///////////
RteTarget* RteProject::GetTarget(const string& targetName) const
{
  if (targetName.empty() && targetName != m_sActiveTarget) {
    return GetActiveTarget();
  }
  map<string, RteTarget*>::const_iterator it = m_targets.find(targetName);
  if (it != m_targets.end())
    return it->second;

  return NULL;
}

RteModel* RteProject::GetTargetModel(const string& targetName) const
{
  auto it = m_targetModels.find(targetName);
  if (it != m_targetModels.end()) {
    return it->second;
  }
  return NULL;
}

RteModel* RteProject::EnsureTargetModel(const string& targetName)
{
  RteModel* model = GetTargetModel(targetName);
  if (!model) {
    model = new RteModel(this);
    m_targetModels[targetName] = model;
  }
  return model;
}

void RteProject::CreateTargetModels()
{
  // create all models for target mentioned in instance infos
  for (auto itpi = m_filteredPackages.begin(); itpi != m_filteredPackages.end(); itpi++) {
    RtePackageInstanceInfo* pi = itpi->second;
    CreateTargetModels(pi);
  }
  for (auto itc = m_components.begin(); itc != m_components.end(); itc++) {
    RteComponentInstance* ci = itc->second;
    CreateTargetModels(ci);
  }

  PropagateFilteredPackagesToTargetModels();
}


void RteProject::CreateTargetModels(RteItemInstance* instance) {
  if (!instance)
    return;
  auto targetInfos = instance->GetTargetInfos();
  for (auto itt = targetInfos.begin(); itt != targetInfos.end(); itt++) {
    const string& targetName = itt->first;
    EnsureTargetModel(targetName);
  }
}


bool RteProject::AddTarget(RteTarget *target) {
  if (!target)
    return false;
  m_targets[target->GetName()] = target;
  target->UpdateFilterModel(); // calls FilterComponents and FillDeviceTree
  return true;
}

RteTarget* RteProject::CreateTarget(RteModel* filteredModel, const string& name, const map<string, string>& attributes)
{
  return new RteTarget(this, filteredModel, name, attributes);
}

bool RteProject::AddTarget(const string& name, const map<string, string>& attributes, bool supported, bool bForceFilterComponents)
{
  if (name.empty())
    return false;
  RteTarget* target = GetTarget(name);
  bool bNewTarget = false;
  if (!target) {
    RteModel* model = GetTargetModel(name);
    if (!model) {
      model = EnsureTargetModel(name);
      PropagateFilteredPackagesToTargetModel(name);
    }
    target = CreateTarget(model, name, attributes);
    m_targets[name] = target;
    bNewTarget = true;
  }

  if (target) {
    target->SetTargetSupported(supported);
    RteAttributes targetAttributes(attributes);
    RteBoardInfo* boardInfo = target->GetBoardInfo();
    if (boardInfo) {
      targetAttributes.AddAttributes(boardInfo->GetAttributes(), false);
    }

    bool changed = target->SetAttributes(targetAttributes);
    if (supported) {
      if (bNewTarget) {
        AddTargetInfo(name);
      }
      if (bNewTarget || changed || bForceFilterComponents) {
        target->UpdateFilterModel(); // calls FilterComponents and FillDeviceTree
      }
    }
    return changed || bNewTarget;
  }
  return false;
}


void RteProject::RemoveTarget(const string& name)
{
  if (name.empty())
    return;

  auto it = m_targets.find(name);
  if (it != m_targets.end()) {
    delete it->second;
    if (it->first == m_sActiveTarget) {
      m_sActiveTarget.clear();
    }
    m_targets.erase(it);
  }
  // model should be deleted after target
  auto itm = m_targetModels.find(name);
  if (itm != m_targetModels.end()) {
    delete itm->second;
    m_targetModels.erase(itm);
  }

  RemoveTargetInfo(name);
}

void RteProject::RenameTarget(const string& oldName, const string& newName)
{
  if (oldName == newName) {
    return; // nothing to do
  }

  // ensure removing newName target
  RemoveTarget(newName);

  auto it = m_targets.find(oldName);
  if (it != m_targets.end()) {
    RteTarget* t = it->second;
    m_targets.erase(it);
    m_targets[newName] = t;
    t->SetName(newName);
    if (oldName == m_sActiveTarget) {
      m_sActiveTarget = newName;
    }
  }
  auto itm = m_targetModels.find(oldName);
  if (itm != m_targetModels.end()) {
    RteModel* m = itm->second;
    m_targetModels.erase(itm);
    m_targetModels[newName] = m;
  }

  RenameTargetInfo(oldName, newName);
}

bool RteProject::SetActiveTarget(const string& targetName)
{
  if (m_sActiveTarget != targetName) {
    m_sActiveTarget = targetName;
    return true;
  }
  return false;
}


void RteProject::ClearTargets()
{
  for (auto it = m_targets.begin(); it != m_targets.end(); it++) {
    delete it->second;
  }
  for (auto itm = m_targetModels.begin(); itm != m_targetModels.end(); itm++) {
    delete itm->second;
  }


  for (auto itc = m_components.begin(); itc != m_components.end(); itc++) {
    RteComponentInstance* ci = itc->second;
    ci->ClearResolved();
  }

  for (auto itp = m_filteredPackages.begin(); itp != m_filteredPackages.end(); itp++) {
    RtePackageInstanceInfo* pi = itp->second;
    pi->ClearResolved();
  }

  m_targets.clear();
  m_targetModels.clear();
  m_sActiveTarget.clear();
  m_bInitialized = false;
}


void RteProject::AddTargetInfo(const string& targetName)
{
  const string& activeTarget = GetActiveTargetName();
  m_packFilterInfos->AddTargetInfo(targetName, activeTarget);

  for (auto itp = m_filteredPackages.begin(); itp != m_filteredPackages.end(); itp++) {
    RtePackageInstanceInfo* pi = itp->second;
    pi->AddTargetInfo(targetName, activeTarget);
  }

  for (auto itg = m_gpdscInfos.begin(); itg != m_gpdscInfos.end(); itg++) {
    RteGpdscInfo* pi = itg->second;
    pi->AddTargetInfo(targetName, activeTarget);
  }

  for (auto itb = m_boardInfos.begin(); itb != m_boardInfos.end(); itb++) {
    RteBoardInfo* bi = itb->second;
    if (bi->IsUsedByTarget(activeTarget) && bi->ResolveBoard(targetName)) {
      bi->AddTargetInfo(targetName, activeTarget);
    }
  }

  for (auto itc = m_components.begin(); itc != m_components.end(); itc++) {
    RteComponentInstance* ci = itc->second;
    ci->AddTargetInfo(targetName, activeTarget);
  }

  for (auto itf = m_files.begin(); itf != m_files.end(); itf++) {
    RteFileInstance* fi = itf->second;
    fi->AddTargetInfo(targetName, activeTarget);
  }
}


bool RteProject::RemoveTargetInfo(const string& targetName)
{
  bool changed = false;
  if (m_packFilterInfos->RemoveTargetInfo(targetName))
    changed = true;

  for (auto itp = m_filteredPackages.begin(); itp != m_filteredPackages.end(); itp++) {
    RtePackageInstanceInfo* pi = itp->second;
    if (pi->RemoveTargetInfo(targetName))
      changed = true;
  }

  for (auto itg = m_gpdscInfos.begin(); itg != m_gpdscInfos.end(); itg++) {
    RteGpdscInfo* pi = itg->second;
    if (pi->RemoveTargetInfo(targetName))
      changed = true;
  }

  for (auto itb = m_boardInfos.begin(); itb != m_boardInfos.end(); itb++) {
    RteBoardInfo* bi = itb->second;
    if (bi->RemoveTargetInfo(targetName))
      changed = true;
  }


  for (auto itc = m_components.begin(); itc != m_components.end(); itc++) {
    RteComponentInstance* ci = itc->second;
    if (ci->RemoveTargetInfo(targetName))
      changed = true;
  }

  for (auto itf = m_files.begin(); itf != m_files.end(); itf++) {
    RteFileInstance* fi = itf->second;
    if (fi->RemoveTargetInfo(targetName))
      changed = true;
  }
  return changed;
}

bool RteProject::RenameTargetInfo(const string& oldName, const string& newName)
{
  bool changed = false;
  if (m_packFilterInfos->RenameTargetInfo(oldName, newName))
    changed = true;

  for (auto itp = m_filteredPackages.begin(); itp != m_filteredPackages.end(); itp++) {
    RtePackageInstanceInfo* pi = itp->second;
    if (pi->RenameTargetInfo(oldName, newName))
      changed = true;
  }

  for (auto itg = m_gpdscInfos.begin(); itg != m_gpdscInfos.end(); itg++) {
    RteGpdscInfo* pi = itg->second;
    if (pi->RenameTargetInfo(oldName, newName))
      changed = true;
  }
  for (auto itb = m_boardInfos.begin(); itb != m_boardInfos.end(); itb++) {
    RteBoardInfo* bi = itb->second;
    if (bi->RenameTargetInfo(oldName, newName)) {
      changed = true;
    }
  }

  for (auto itc = m_components.begin(); itc != m_components.end(); itc++) {
    RteComponentInstance* ci = itc->second;
    if (ci->RenameTargetInfo(oldName, newName))
      changed = true;
  }

  for (auto itf = m_files.begin(); itf != m_files.end(); itf++) {
    RteFileInstance* fi = itf->second;
    if (fi->RenameTargetInfo(oldName, newName))
      changed = true;
  }
  return changed;
}

void RteProject::FilterComponents()
{
  RteTarget* activeTarget = GetActiveTarget();
  // iterate through full list selecting components that pass target filter
  for (auto it = m_targets.begin(); it != m_targets.end(); it++) {
    if (it->second == activeTarget) // skip active target, evaluate it at the end
      continue;
    FilterComponents(it->second);
  }
  // apply active target filter as the last one
  FilterComponents(activeTarget);
}

void RteProject::ClearFilteredPackages()
{
  for (auto it = m_filteredPackages.begin(); it != m_filteredPackages.end(); it++) {
    delete it->second;
  }
  m_packFilterInfos->Clear();
  m_filteredPackages.clear();
}


void RteProject::PropagateFilteredPackagesToTargetModels()
{
  for (auto it = m_targetModels.begin(); it != m_targetModels.end(); it++) {
    const string& targetName = it->first;
    PropagateFilteredPackagesToTargetModel(targetName);
  }
}


void RteProject::PropagateFilteredPackagesToTargetModel(const string& targetName)
{
  if (!m_globalModel)
    return;
  RteModel* model = EnsureTargetModel(targetName);

  RteInstanceTargetInfo* info = m_packFilterInfos->GetTargetInfo(targetName);
  bool bUseAll = !info || !info->IsExcluded();

  RtePackageFilter& filter = model->GetPackageFilter();
  RteAttributesMap fixedPacks;
  RteAttributesMap latestPacks;
  for (auto itpi = m_filteredPackages.begin(); itpi != m_filteredPackages.end(); itpi++) {
    RtePackageInstanceInfo* pi = itpi->second;
    if (!pi->IsFilteredByTarget(targetName))
      continue;

    if (pi->IsExcluded(targetName)) {
      continue;
    }

    VersionCmp::MatchMode mode = pi->GetVersionMatchMode(targetName);
    if (mode == VersionCmp::MatchMode::FIXED_VERSION) {
      const string& id = itpi->first;
      fixedPacks[id] = *pi;
    } else {
      const string& id = pi->GetPackageID(false);
      latestPacks[id] = *pi;
    }
  }
  filter.SetSelectedPackages(fixedPacks);
  filter.SetLatestPacks(latestPacks);
  if (!fixedPacks.empty() || !latestPacks.empty() || !bUseAll)
    bUseAll = false;
  filter.SetUseAllPacks(bUseAll);
  model->FilterModel(m_globalModel, NULL);
}


bool RteProject::CollectFilteredPackagesFromTargets()
{
  bool bModified = false;
  // remove/update
  for (auto itpi = m_filteredPackages.begin(); itpi != m_filteredPackages.end();) {
    auto itcurrent = itpi++;
    const string& id = itcurrent->first;
    RtePackageInstanceInfo* pi = itcurrent->second;
    for (auto it = m_targets.begin(); it != m_targets.end(); it++) {
      RteTarget* target = it->second;
      const string& targetName = it->first;
      const RtePackageFilter& filter = target->GetPackageFilter();

      if (filter.IsUseAllPacks()) {
        if (pi->RemoveTargetInfo(targetName))
          bModified = true;
        continue;
      }

      bool bExcluded = filter.IsPackageExcluded(id);
      if (bExcluded) {
        if (pi->RemoveTargetInfo(targetName))
          bModified = true;
      } else {
        bool bSelected = filter.IsPackageSelected(id);
        if (pi->SetExcluded(false, targetName))
          bModified = true;
        if (!pi->GetTargetInfo(targetName))
          bModified = true;
        pi->AddTargetInfo(targetName);
        if (pi->SetUseLatestVersion(!bSelected, targetName))
          bModified = true;
      }
    }
    if (pi->GetTargetInfos().empty()) {
      delete pi;
      m_filteredPackages.erase(itcurrent);
    }
  }

  // add new
  for (auto it = m_targets.begin(); it != m_targets.end(); it++) {
    RteTarget* target = it->second;
    const string& targetName = it->first;
    const RtePackageFilter& filter = target->GetPackageFilter();
    if (filter.AreAllExcluded()) {
      RteInstanceTargetInfo* info = m_packFilterInfos->EnsureTargetInfo(targetName);
      info->SetExcluded(true);
    } else {
      m_packFilterInfos->RemoveTargetInfo(targetName);
    }

    const RteAttributesMap& latestPacks = filter.GetLatestPacks();
    for (auto ite = latestPacks.begin(); ite != latestPacks.end(); ite++) {
      const RteAttributes& attr = ite->second;
      const string& id = attr.GetPackageID(true);
      auto itpi = m_filteredPackages.find(id);
      RtePackageInstanceInfo* pi = NULL;
      if (itpi != m_filteredPackages.end()) {
        pi = itpi->second;
      }
      if (!pi) {
        pi = new RtePackageInstanceInfo(this);
        m_filteredPackages[id] = pi;
        pi->SetAttributes(attr);
        bModified = true;
      } else if (!pi->GetTargetInfo(targetName))
        bModified = true;

      pi->AddTargetInfo(targetName);
      if (pi->SetUseLatestVersion(true, targetName))
        bModified = true;
    }

    const RteAttributesMap& packs = filter.GetSelectedPackages();
    for (auto itp = packs.begin(); itp != packs.end(); itp++) {
      const string& id = itp->first;
      const RteAttributes& attr = itp->second;
      auto itpi = m_filteredPackages.find(id);
      RtePackageInstanceInfo* pi = NULL;
      if (itpi != m_filteredPackages.end()) {
        pi = itpi->second;
      }
      if (!pi) {
        pi = new RtePackageInstanceInfo(this);
        m_filteredPackages[id] = pi;
        pi->SetAttributes(attr);
        bModified = true;
      } else if (!pi->GetTargetInfo(targetName))
        bModified = true;

      pi->AddTargetInfo(targetName);
      if (pi->SetUseLatestVersion(false, targetName))
        bModified = true;
    }
  }
  return bModified;
}



void RteProject::FilterComponents(RteTarget* target)
{
  if (target)
    target->UpdateFilterModel();
}

void RteProject::EvaluateComponentDependencies(RteTarget* target)
{
  if (!target)
    target = GetActiveTarget();
  if (target)
    target->EvaluateComponentDependencies();
}

bool RteProject::AreDependenciesResolved(RteTarget* target) const
{
  if (!target)
    target = GetActiveTarget();
  RteItem::ConditionResult result = target->GetDependencySolver()->GetConditionResult();
  return result >= FULFILLED;
}



bool RteProject::ResolveDependencies(RteTarget* target)
{
  if (!target)
    target = GetActiveTarget();

  RteDependencySolver* solver = target->GetDependencySolver();

  RteItem::ConditionResult result = solver->ResolveDependencies();
  return result >= FULFILLED;
}


void RteProject::ClearSelected()
{
  map<string, RteTarget*>::iterator it;
  for (it = m_targets.begin(); it != m_targets.end(); it++) {
    RteTarget* target = it->second;
    target->ClearSelectedComponents();
  }
}

void RteProject::ClearUsedComponents()
{
  map<string, RteTarget*>::iterator it;
  for (it = m_targets.begin(); it != m_targets.end(); it++) {
    RteTarget* target = it->second;
    target->ClearUsedComponents();
  }
}

void RteProject::PropagateActiveSelectionToAllTargets()
{
  RteTarget* activeTarget = GetActiveTarget();
  map<string, RteTarget*>::iterator it;
  for (it = m_targets.begin(); it != m_targets.end(); it++) {
    RteTarget* target = it->second;
    if (target != activeTarget && target->IsTargetSupported())
      target->SetSelectionFromTarget(activeTarget);
  }
}


void RteProject::CollectMissingPacks(RteItemInstance* inst)
{
  if (!inst)
    return;
  if (inst->IsGenerated())
    return;


  const RteInstanceTargetInfoMap& tiMap = inst->GetTargetInfos();
  for (auto it = tiMap.begin(); it != tiMap.end(); it++) {
    const string& targetName = it->first;
    RtePackage* pack = inst->GetEffectivePackage(targetName);
    if (pack != NULL)
      continue;
    string packId = inst->GetEffectivePackageID(targetName);
    const string& url = inst->GetURL();
    t_missingPackIds[packId] = url;
    t_missingPackTargets.insert(targetName);
  }
}


void RteProject::CollectMissingPacks()
{
  ClearMissingPacks();
  for (auto itpi = m_filteredPackages.begin(); itpi != m_filteredPackages.end(); itpi++) {
    CollectMissingPacks(itpi->second);
  }
  // collect missing pack from components
  for (auto itc = m_components.begin(); itc != m_components.end(); itc++) {
    CollectMissingPacks(itc->second);
  }
}

bool RteProject::Validate()
{
  m_bValid = true; // assume valid
  bool bValid = true;
  ClearErrors();


  const string& targetName = GetActiveTargetName();
  RteTarget* target = GetActiveTarget();
  if (!target)
    return true; // nothing to do
  target->ClearMissingPacks();

  // checki if all required gpdsc files are available
  for (auto itg = m_gpdscInfos.begin(); itg != m_gpdscInfos.end(); itg++) {
    RteGpdscInfo* gi = itg->second;
    RteModel* m = gi->GetGeneratorModel();
    if (m)
      continue; // loaded
    bValid = false;
    string fileName = gi->GetAbsolutePath();
    error_code ec;
    if (!fs::exists(fileName, ec)) {
      string msg = "Error #545: Required gpdsc file '";
      msg += fileName;
      msg += "' is missing";
      m_errors.push_back(msg);
    } else {
      string msg = "Error #546: Required gpdsc file '";
      msg += fileName;
      msg += "' is not loaded, errors by load";
      m_errors.push_back(msg);
    }
  }


  // check if all fixed packs are installed
  for (auto itpi = m_filteredPackages.begin(); itpi != m_filteredPackages.end(); itpi++) {
    RtePackageInstanceInfo* pi = itpi->second;
    if (!pi->IsUsedByTarget(targetName))
      continue;
    VersionCmp::MatchMode mode = pi->GetVersionMatchMode(targetName);
    string packId = pi->GetPackageID(mode == VersionCmp::MatchMode::FIXED_VERSION);
    const string& url = pi->GetURL();
    //if(!IsPackageUsed( packId, targetName, mode == VersionCmp::FIXED_VERSION))
    // continue;

    RtePackage* pack = pi->GetResolvedPack(targetName);
    if (!pack) {
      if (target->IsPackMissing(packId))
        continue;

      string msg = "Error #544: Required Software Pack '";
      msg += packId;
      msg += "' is not installed";
      m_errors.push_back(msg);
      target->AddMissingPackId(packId, url);
      bValid = false;
    }
  }

  // check if device support is installed
  if (target->GetDevice() == NULL) {
    string packId = GetEffectivePackageID(target->GetAttribute("pack"), targetName);
    string url = target->GetAttribute("url");
    target->AddMissingPackId(packId, url);

    const string& deviceName = target->GetAttribute("Dname");
    string vendor = DeviceVendor::GetCanonicalVendorName(target->GetAttribute("Dvendor"));

    string msg = "Error #543: Device ";
    msg += deviceName;
    msg += "(";
    msg += vendor;
    msg += ")";
    msg += " not found, pack '";
    msg += packId;
    msg += "' is not installed";
    m_errors.push_back(msg);
  }

  // it is enough to check if all components and APIs are available
  map<string, RteComponentInstance*>::const_iterator itc;
  for (itc = m_components.begin(); itc != m_components.end(); itc++) {
    RteComponentInstance* ci = itc->second;
    if (!ci || !ci->IsUsedByTarget(targetName))
      continue;

    RteComponent* c = ci->GetResolvedComponent(targetName);
    if (!c) {
      string packId = ci->GetEffectivePackageID(targetName);
      bool bPackMissing = target->IsPackMissing(packId);
      if (!bPackMissing) {
        packId = RtePackage::ReleaseIdFromId(packId);
      }
      const string& url = ci->GetURL();
      bValid = false;
      ConditionResult res = ci->GetResolveResult(targetName);
      if (!bPackMissing && (res == UNAVAILABLE || res == UNAVAILABLE_PACK)) {
        string msg = "Error #540: '";
        msg += ci->GetEffectiveDisplayName(targetName);
        msg += "' ";
        if (ci->IsApi()) {
          msg += "API";
        } else {
          msg += "component";
        }
        msg += " is not available for target '";
        msg += targetName;
        msg += "'";
        if (res == UNAVAILABLE_PACK) {
          msg += ", pack \'";
          msg += packId;
          msg += "\' is not selected";
        }
        m_errors.push_back(msg);
      } else {
        string msg = "Error #541: '";
        msg += ci->GetFullDisplayName();
        msg += "' ";
        if (ci->IsApi()) {
          msg += "API";
        } else {
          msg += "component";
        }
        msg += " is missing (previously found in pack '";
        msg += packId;
        msg += "')";
        m_errors.push_back(msg);
        target->AddMissingPackId(packId, url);
      }
    } else if (!c->IsApi()) {
      RteItem::ConditionResult apiResult = c->IsApiAvailable(target);
      if (apiResult == RteItem::MISSING_API) {
        bValid = false;
        string msg = "Error #542: Component '";
        msg += c->GetFullDisplayName();
        msg += "': API version '";
        msg += c->GetApiVersionString();
        msg += "' or higher is required. ";
        msg += "API definition is missing (no pack ID is available)";
        m_errors.push_back(msg);
      } else if (apiResult == RteItem::MISSING_API_VERSION) {
        bValid = false;
        RteApi* api = c->GetApi(target, false);
        string msg = "Error #552: Component '";
        msg += c->GetFullDisplayName();
        msg += "': API version '";
        msg += c->GetApiVersionString();
        msg += "' or higher is required.";
        if (api) {
          msg += " (Version '";
          msg += api->GetApiVersionString();
          msg += "' is found in pack '";
          msg += api->GetPackageID(true);
          msg += "').";
        }
        m_errors.push_back(msg);
      }
    }
  }
  return bValid;
}


void RteProject::UpdateModel()
{
  if (!m_globalModel)
    return;
  ClearUsedComponents();
  ClearSelected();
  map<string, RteComponentInstance*>::iterator itc;
  for (itc = m_components.begin(); itc != m_components.end(); itc++) {
    RteComponentInstance* ci = itc->second;

    const RteInstanceTargetInfoMap& targetInfos = ci->GetTargetInfos();
    for (auto it = targetInfos.begin(); it != targetInfos.end(); it++) {
      const string& targetName = it->first;
      RteTarget* target = GetTarget(targetName);
      if (target) {
        int count = it->second->GetInstanceCount();
        target->SetComponentUsed(ci, count);
      }
    }
  }
  for (auto it = m_targets.begin(); it != m_targets.end(); it++) {
    RteTarget* target = it->second;
    target->CollectSelectedComponentAggregates();
    EvaluateComponentDependencies(target);
    target->CollectFilteredFiles();
  }
}

bool RteProject::Construct(XMLTreeElement* xmlTree)
{
  // we are not interested in XML attributes for model Instance
  const list<XMLTreeElement*>& docs = xmlTree->GetChildren();
  list<XMLTreeElement*>::const_iterator it = docs.begin();
  bool success = false;
  if (it != docs.end()) { // there should be only one
    XMLTreeElement* xmlElement = *it;
    m_tag = xmlElement->GetTag();
    success = ProcessXmlChildren(xmlElement);
    m_ID = "RTE";
    m_tag = "RTE";
  }
  UpdateClasses();
  return success;
}

bool RteProject::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  const string& tag = xmlElement->GetTag();
  if (tag == "components" || tag == "apis" || tag == "files" || tag == "packages" || tag == "gpdscs" || tag == "boards") {
    return ProcessXmlChildren(xmlElement); // will recurcively call this function (processed in the next if clauses)
  } else if (tag == "component" || tag == "api") {
    RteComponentInstance* ci = new RteComponentInstance(this);
    if (ci->Construct(xmlElement)) {
      AddItem(ci);
      m_components[ci->GetID()] = ci;
      return true;
    }
    delete ci;
    return false;
  } else if (tag == "file") {
    RteFileInstance* fi = new RteFileInstance(this);
    if (fi->Construct(xmlElement)) {
      // in old packs we also same files with other case, do not load duplicated removed files in this case
      RteFileInstance* oldFi = GetFileInstance(fi->GetID());
      if (!oldFi || (oldFi->IsRemoved() && !fi->IsRemoved())) {
        DeleteFileInstance(oldFi);
        AddItem(fi);
        m_files[fi->GetID()] = fi;
        return true;
      }
    }
    delete fi;
    return true; // fi can only be deleted if the item is not a config one
  } else if (tag == "package") {
    RtePackageInstanceInfo* pi = new RtePackageInstanceInfo(this);
    if (pi->Construct(xmlElement)) {
      m_filteredPackages[pi->GetPackageID(true)] = pi;
      return true;
    }
    delete pi;
    return true;
  } else if (tag == "gpdsc") {
    RteGpdscInfo* gi = new RteGpdscInfo(this);
    if (gi->Construct(xmlElement)) {
      m_gpdscInfos[gi->GetAbsolutePath()] = gi;
      return true;
    }
    delete gi;
    return true;
  } else if (tag == "board") {
    RteBoardInfo* bi = new RteBoardInfo(this);
    if (bi->Construct(xmlElement)) {
      m_boardInfos[bi->GetDisplayName()] = bi;
      return true;
    }
    delete bi;
    return true;
  } else if (tag == "filter") {
    m_packFilterInfos->Construct(xmlElement);
    return true;
  }

  return RteItem::ProcessXmlElement(xmlElement);
}


void RteProject::CreateXmlTreeElementContent(XMLTreeElement* parentElement) const
{
  XMLTreeElement *e = NULL;

  if (!m_filteredPackages.empty() || m_packFilterInfos->GetTargetCount() > 0) {
    e = new XMLTreeElement(parentElement);
    e->SetTag("packages");

    m_packFilterInfos->CreateXmlTreeElement(e);

    for (auto itp = m_filteredPackages.begin(); itp != m_filteredPackages.end(); itp++) {
      RtePackageInstanceInfo* pi = itp->second;
      // store only fixed and excluded
      if (pi && !pi->IsExcludedForAllTargets()) {
        pi->SetTag("package");
        pi->CreateXmlTreeElement(e);
      }
    }
  }

  if (!m_gpdscInfos.empty()) {
    e = new XMLTreeElement(parentElement);
    e->SetTag("gpdscs");
    for (auto itp = m_gpdscInfos.begin(); itp != m_gpdscInfos.end(); itp++) {
      RteGpdscInfo* pi = itp->second;
      if (pi) {
        pi->SetTag("gpdsc");
        pi->CreateXmlTreeElement(e);
      }
    }
  }

  if (!m_boardInfos.empty()) {
    e = new XMLTreeElement(parentElement);
    e->SetTag("boards");
    for (auto itb = m_boardInfos.begin(); itb != m_boardInfos.end(); itb++) {
      RteBoardInfo* bi = itb->second;
      if (bi && bi->GetTargetCount() > 0) {
        bi->SetTag("board");
        bi->CreateXmlTreeElement(e);
      }
    }
  }

  e = new XMLTreeElement(parentElement);
  e->SetTag("apis");
  map<string, RteComponentInstance*>::const_iterator itc;
  for (itc = m_components.begin(); itc != m_components.end(); itc++) {
    RteComponentInstance* ci = itc->second;
    if (ci && ci->IsApi())
      ci->CreateXmlTreeElement(e);
  }
  e = new XMLTreeElement(parentElement);
  e->SetTag("components");
  for (itc = m_components.begin(); itc != m_components.end(); itc++) {
    RteComponentInstance* ci = itc->second;
    if (ci && !ci->IsApi() && !ci->IsGenerated())
      ci->CreateXmlTreeElement(e);
  }
  e = new XMLTreeElement(parentElement);
  e->SetTag("files");
  map<string, RteFileInstance*>::const_iterator itf;
  for (itf = m_files.begin(); itf != m_files.end(); itf++) {
    RteFileInstance* fi = itf->second;
    fi->CreateXmlTreeElement(e);
  }
}

void RteProject::GetUsedComponents(RteComponentMap & components, const string& targetName) const
{
  for (auto it = m_components.begin(); it != m_components.end(); it++) {
    RteComponentInstance* ci = it->second;
    RteComponent* c = ci->GetResolvedComponent(targetName);
    if (c) {
      const string& id = c->GetID();
      components[id] = c;
    }
  }
}

// returns used components for the entire project
void RteProject::GetUsedComponents(RteComponentMap& components) const
{
  for (auto it = m_targets.begin(); it != m_targets.end(); it++) {
    const string& targetName = it->first;
    GetUsedComponents(components, targetName);
  }
}

bool RteProject::IsComponentUsed(const string& aggregateId, const string& targetName) const
{
  for (auto it = m_components.begin(); it != m_components.end(); it++) {
    RteComponentInstance* ci = it->second;
    if (ci->GetComponentAggregateID() == aggregateId)
      if (ci->IsUsedByTarget(targetName))
        return true;
  }
  return false;
}

bool RteProject::IsPackageUsed(const string& packId, const string& targetName, bool bFullId) const
{
  RteTarget* t = GetTarget(targetName);
  if (t) {
    RteDeviceItem* device = t->GetDevice();
    if (device && packId == device->GetPackageID(bFullId)) {
      return true;
    }
    RteBoard* board = t->GetBoard();
    if (board && packId == board->GetPackageID(bFullId)) {
      return true;
    }
  }

  for (auto it = m_components.begin(); it != m_components.end(); it++) {
    RteComponentInstance* ci = it->second;
    if (ci->IsUsedByTarget(targetName)) {
      RteComponent* c = ci->GetResolvedComponent(targetName);
      if (c) {
        if (c->GetPackageID(bFullId) == packId)
          return true;
      }
      if (ci->GetPackageID(bFullId) == packId)
        return true;
    }
  }
  return false;
}


void RteProject::GetUsedPacks(RtePackageMap& packs, const string& targetName) const
{
  RteTarget* t = GetTarget(targetName);
  if (!t)
    return;
  RteDeviceItem* device = t->GetDevice();
  if (device) {
    RtePackage* pack = device->GetPackage();
    if (pack) {
      const string& id = pack->GetID();
      packs[id] = pack;
    }
  }
  RteBoard* board = t->GetBoard();
  if (board) {
    RtePackage* pack = board->GetPackage();
    if (pack) {
      const string& id = pack->GetID();
      packs[id] = pack;
    }
  }


  for (auto it = m_components.begin(); it != m_components.end(); it++) {
    RteComponentInstance* ci = it->second;
    if (!ci->IsUsedByTarget(targetName))
      continue;
    RtePackage* pack = ci->GetEffectivePackage(targetName);
    if (pack) {
      const string& id = pack->GetID();
      packs[id] = pack;
    }
  }
}

bool RteProject::HasProjectGroup(const string& group) const
{
  map<string, RteTarget*>::const_iterator it;
  for (it = m_targets.begin(); it != m_targets.end(); it++) {
    RteTarget* t = it->second;
    if (t->HasProjectGroup(group))
      return true;
  }
  return false;
}

bool RteProject::HasProjectGroup(const string& group, const string& target) const
{
  RteTarget* t = GetTarget(target);
  if (t) {
    return t->HasProjectGroup(group);
  }
  return false;
}

bool RteProject::IsProjectGroupEnabled(const string& group, const string& target) const
{
  if (HasProjectGroup(group, target)) {
    string className;
    if (group.find("::") == 0)
      className = group.substr(2);
    else
      className = group;
    RteComponentInstanceGroup* g = GetClassGroup(className);
    if (g)
      return g->IsUsedByTarget(target);
  }
  return false;
}


bool RteProject::HasFileInProjectGroup(const string& group, const string& file) const
{
  map<string, RteTarget*>::const_iterator it;
  for (it = m_targets.begin(); it != m_targets.end(); it++) {
    RteTarget* t = it->second;
    if (t->HasFileInProjectGroup(group, file))
      return true;
  }
  return false;
}

bool RteProject::HasFileInProjectGroup(const string& group, const string& file, const string& target) const
{
  RteTarget* t = GetTarget(target);
  if (t) {
    return t->HasFileInProjectGroup(group, file);
  }
  return false;
}

string RteProject::GetFileComment(const string& group, const string& file) const
{
  map<string, RteTarget*>::const_iterator it;
  for (it = m_targets.begin(); it != m_targets.end(); it++) {
    RteTarget* t = it->second;
    string comment = t->GetFileComment(group, file);
    if (!comment.empty())
      return comment;
  }
  return EMPTY_STRING;
}

bool RteProject::ShouldUpdateRte() const
{
    return GetAttributeAsBool("update-rte-files", true);
}

const RteFileInfo* RteProject::GetFileInfo(const string& groupName, const string& file, const string& targetName) const
{
  RteTarget* t = GetTarget(targetName);
  if (t) {
    return t->GetFileInfo(groupName, file);
  }
  return NULL;
}

// End of RteProject.cpp
