/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteProject.cpp
* @brief CMSIS RTE Instance in uVision Project
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
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

#include <sstream>
using namespace std;

const std::string RteProject::DEFAULT_RTE_FOLDER = "RTE";


RteLicenseInfo::RteLicenseInfo(RteItem* parent) :
  RteItem("license", parent)
{
}

std::string RteLicenseInfo::ToString(unsigned indent)
{
  stringstream ss;
  ss << RteUtils::GetIndent(indent) << "- license: " << ConstructLicenseTitle(this) << endl;
  indent += 2;
  const string& license_agreement = GetAttribute("agreement");
  if (!license_agreement.empty()) {
    ss << RteUtils::GetIndent(indent) << "license-agreement: " << license_agreement << endl;
  }
  ss << RteUtils::GetIndent(indent) << "packs:" << endl;
  for (auto& packId : m_packIDs) {
    ss << RteUtils::GetIndent(indent) << "- pack: " << packId << endl;
  }
  if (!m_componentIDs.empty()) {
    ss << RteUtils::GetIndent(indent) << "components:" << endl;
    for (auto& compId : m_componentIDs) {
      ss << RteUtils::GetIndent(indent) << "- component: " << compId << endl;
    }
  }
  return ss.str();
}

std::string RteLicenseInfo::ConstructLicenseTitle(const RteItem* license)
{
  string name = license->GetAttribute("spdx");
  if (name.empty()) {
    name = license->GetAttribute("title");
    if (!name.empty()) {
      name = "<proprietary> " + name;
    } else {
      name = "<unknown>";
    }
  }
  return name;
}

std::string RteLicenseInfo::ConstructLicenseID(const RteItem* license)
{
  string id = license->GetAttribute("spdx");
  if (id.empty()) {
    id = ConstructLicenseTitle(license) + "(" + license->GetPackageID()+ ")";
  }
  return id;
}

RteLicenseInfoCollection::~RteLicenseInfoCollection()
{
  Clear();
}

void RteLicenseInfoCollection::Clear()
{
  for (auto [_, ptr] : m_LicensInfos) {
    delete ptr;
  }
  m_LicensInfos.clear();
}

std::string RteLicenseInfoCollection::ToString()
{
  if (m_LicensInfos.empty()) {
    return RteUtils::EMPTY_STRING;
  }
  stringstream ss;
  ss << "licenses:" << endl;
  for (auto& [key, info] : m_LicensInfos) {
    ss << info->ToString(2) << endl;
  }
  return ss.str();
}


void RteLicenseInfoCollection::AddLicenseInfo(RteItem* item)
{
  if (!item) {
    return;
  }
  RteItem* licenseSet = item->GetLicenseSet();
  if (licenseSet) {
    for (RteItem* license : licenseSet->GetChildren()) {
      EnsureLicenseInfo(item, license);
    }
  } else {
    EnsureLicenseInfo(item, nullptr);
  }
}

RteLicenseInfo* RteLicenseInfoCollection::EnsureLicenseInfo(RteItem* item, RteItem* license)
{
  RtePackage* pack = item->GetPackage();
  if (!pack) {
    return nullptr;
  }
  string licId = license? RteLicenseInfo::ConstructLicenseID(license) : RteLicenseInfo::ConstructLicenseID(pack);

  auto it = m_LicensInfos.find(licId);
  RteLicenseInfo* info = nullptr;;
  if (it == m_LicensInfos.end()) {
    info = new RteLicenseInfo();
    m_LicensInfos[licId] = info;
    string licFile;
    if (license) {
      info->SetAttributes(*license);
      if(!info->HasAttribute("spdx")) {
        licFile = license->GetName();
      }
    } else {
      info->AddAttribute("pack", pack->GetID());
      licFile = pack->GetChildText("license");
    }
      if (!licFile.empty()) {
        info->AddAttribute("agreement", "${CMSIS_PACK_ROOT}/" + pack->GetPackagePath(true) + licFile);
    }
  } else {
    info = it->second;
  }
  info->AddPackId(pack->GetID());
  if (dynamic_cast<RteComponent*>(item)) {
    info->AddComponentId(item->GetComponentID(true));
  }
  return info;
}


////////////////////////////
RteProject::RteProject() :
  RteRootItem(nullptr),
  m_globalModel(nullptr),
  m_callback(nullptr),
  m_packFilterInfos(nullptr),
  m_classes(nullptr),
  m_nID(0),
  m_bInitialized(false),
  t_bGpdscListModified(false)
{
  m_tag = "RTE";
  m_packFilterInfos = new RteItemInstance(this);
  m_packFilterInfos->SetTag("filter");
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
  m_forcedFiles.clear();
  ClearFilteredPackages();

  for (auto [_, gi] : m_gpdscInfos) {
    delete gi;
  }
  m_gpdscInfos.clear();

  for (auto [_, bi] : m_boardInfos) {
    delete bi;
  }
  m_boardInfos.clear();

  RteRootItem::Clear();
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
  for (auto [_, ci] : m_components) {
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
  for (auto [_, ci] : m_components) {
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
  for (it = m_files.begin(); it != m_files.end(); ++it) {
    if ( RteUtils::EqualNoCase(id, it->first)) {
      return it->second;
    }
  }
  return NULL;
}

void RteProject::GetFileInstancesForComponent(RteComponentInstance* ci, const string& targetName, map<string, RteFileInstance*>& configFiles) const
{
  for (auto [id, fi] : m_files) {
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
  // use the original pack of bootstrap component if available
  if (c->IsGenerated() && c->HasAttribute("selectable") ) {
    RteItem* packInfo = c->GetFirstChild("package");
    if (packInfo) {
      ci->SetPackageAttributes(packInfo->GetAttributes());
    }
  }

  if (instanceCount > 0) {
    // add gpdsc info if needed
    AddGpdscInfo(c, target);
  }

  return ci;
}

void RteProject::AddCprjComponents(const Collection<RteItem*>& selItems, RteTarget* target,
                                  set<RteComponentInstance*>& unresolvedComponents)
{
  map<string, string> configFileVersions;
  // first try to resolve the components
  for (auto item : selItems) {
    // Resolve component
    RteComponentInstance* ci = AddCprjComponent(item, target);
    RteComponent* component = ci->GetResolvedComponent(target->GetName());
    int instances = item->GetAttributeAsInt("instances", 1);
    target->SetComponentUsed(ci, instances);
    if (component) {
      target->SelectComponent(component, instances, false, true);
      for (auto f : item->GetChildren()) {
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
      unresolvedComponents.insert(ci);
    }
  }
  Apply();

  // update file versions
  for (auto [instanceName, fi] : m_files) {
    if(fi->IsRemoved()) {
      continue;
    }
    string version;
    auto rteFile = fi->GetFile(target->GetName());
    auto itver = configFileVersions.find(instanceName); // file in cprj can be specified by its instance name
    if (itver == configFileVersions.end())
      itver = configFileVersions.find(fi->GetName()); // or by original name
    if (itver != configFileVersions.end()) {
      version = itver->second;
    } else if(rteFile) {
      // Fall back to the version noted in the pack
      version = rteFile->GetVersionString();
    }
    UpdateFileInstanceVersion(fi, version);
    UpdateConfigFileBackups(fi, rteFile);
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
    VersionCmp::MatchMode mode = VersionCmp::MatchModeFromString(item->GetAttribute("versionMatchMode"));
    if (mode == VersionCmp::MatchMode::ENFORCED_VERSION) {
      info->SetVersionMatchMode(VersionCmp::MatchMode::ENFORCED_VERSION);
    } else {
      info->SetVersionMatchMode(VersionCmp::MatchMode::FIXED_VERSION);
    }
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
  ci->AddAttribute("layer", item->GetAttribute("layer"), false);
  ci->AddAttribute("rtedir", item->GetAttribute("rtedir"), false);
  ci->AddAttribute("gendir", item->GetAttribute("gendir"), false);
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


void RteProject::AddComponentFiles(RteComponentInstance* ci, RteTarget* target)
{
  const string& targetName = target->GetName();
  RteComponent* c = ci->GetResolvedComponent(targetName);
  if (!c)
    return; // not resolved => nothing to add
  int instanceCount = ci->GetInstanceCount(targetName);
  bool excluded = ci->IsExcluded(targetName);
  // add files
  const set<RteFile*>& files = target->GetFilteredFiles(c);
  for (auto f : files) {
    if (!f)
      continue;
    if (f->IsConfig()) {
      for (int i = 0; i < instanceCount; i++) {
        RteFileInstance* fi = AddFileInstance(ci, f, i, target);
        fi->SetExcluded(excluded, targetName);
      }
    } else if (f->IsForcedCopy()) {
      m_forcedFiles.insert(f);
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
  if (bExists) {
    UpdateFileInstanceVersion(fi, savedVersion);
  }
  UpdateConfigFileBackups(fi, f);
}

void RteProject::WriteInstanceFiles(const std::string& targetName)
{
  for (auto [fileID, fi] : m_files) {
    if (fi->IsRemoved() || fi->GetTargetCount() == 0) {
      RemoveFileInstance(fileID);
    } else if (fi->IsUsedByTarget(targetName)){
      RteFile* f = fi->GetFile(targetName);
      if (f && !RteFsUtils::Exists(fi->GetAbsolutePath())) {
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
  if (!f || !fi) {
    return;
  }

  string src = f->GetOriginalAbsolutePath();
  string absPath = RteFsUtils::AbsolutePath(fi->GetAbsolutePath()).generic_string();
  string dir = RteUtils::ExtractFilePath(absPath, false);
  string name = RteUtils::ExtractFileName(absPath);
  const string& baseVersion = fi->GetAttribute("version"); // explicitly check the file instance version
  const string& updateVersion = f->GetVersionString();
  string baseFile = RteUtils::AppendFileBaseVersion(absPath, baseVersion);
  if (!RteFsUtils::Exists(baseFile)) {
    // create base file if possible
    if (baseVersion == updateVersion) {
      // we can use current version as base one
      RteFsUtils::CopyMergeFile(src, baseFile, fi->GetInstanceIndex(), false);
      RteFsUtils::SetFileReadOnly(baseFile, true);
    } else {
      baseFile.clear(); //no such file
    }
  }
  // copy current file if version differs
  string updateFile = RteUtils::AppendFileUpdateVersion(absPath, updateVersion);
  if (!baseFile.empty() && baseVersion != updateVersion)  { // only copy update if base exists
    RteFsUtils::CopyMergeFile(src, updateFile, fi->GetInstanceIndex(), false);
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
  RemoveChild(fi, true);
}


void RteProject::AddGeneratedComponents()
{
  for (auto [_, gi] : m_gpdscInfos) {
    RtePackage* gpdscPack = gi->GetGpdscPack();
    if (!gpdscPack || !gpdscPack->GetComponents())
      continue;
    for (auto [targetName, target] : m_targets) {
      if (!gi->IsUsedByTarget(targetName))
        continue;
      for (auto item : gpdscPack->GetComponents()->GetChildren()) {
        RteComponent* c = dynamic_cast<RteComponent*>(item);
        AddComponent(c, 1, target, nullptr);
      }
    }
  }
}

void RteProject::RemoveGeneratedComponents() {
  // mark all generated components as removed (flag will be reset later, but only for those that remain in the list)
  for (auto [_, ci] : m_components)
  {
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
  for (auto [_, ci] : m_components)
  {
    ci->SetRemoved(true);
  }

  // add/update components
  for (auto [targetName, target] : m_targets) {

    //// check for removed generated components
    set<RteComponentAggregate*> unselectedGpdscCompoinents;
    target->GetUnSelectedGpdscAggregates(unselectedGpdscCompoinents);
    for (auto ua : unselectedGpdscCompoinents) {
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
    for (auto [a, count] : components) {
      RteComponent* c = a->GetComponent();
      RteComponentInstance* ci = a->GetComponentInstance();

      if (c) {
        if (ci && c->IsGenerated()) {
          const string& gpdsc = c->GetPackage()->GetPackageFileName();
          RteGpdscInfo* gpdscInfo = GetGpdscInfo(gpdsc);
          if (!gpdscInfo) {
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
    for (auto [gpdscFile , gpdscInfo] : m_gpdscInfos) {
      if (!target->IsGpdscUsed(gpdscFile)) {
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
  for (auto a : modifiedAggregates) {
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
      for (auto child : a->GetChildren()) {
        RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(child);
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

    for (auto child : a->GetChildren()) {
      RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(child);
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
        for (auto [targetName, info] : infos) {
          if (targetName != activeTargetName) {
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
    RemoveGeneratedComponents();
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
  m_forcedFiles.clear();
  for (auto itt = m_targets.begin(); itt != m_targets.end(); ++itt) {
    RteTarget* target = itt->second;
    for (auto itc = m_components.begin(); itc != m_components.end(); ++itc) {
      RteComponentInstance* ci = itc->second;
      AddComponentFiles(ci, target);
    }
  }

  CollectSettings();
  // copy files and create headers if update is enabled
  UpdateRte();
}

void RteProject::UpdateRte() {
  if (!ShouldUpdateRte())
    return;
  GenerateRteHeaders();
  for(auto itt = m_targets.begin(); itt != m_targets.end(); ++itt) {
    WriteInstanceFiles(itt->first);
  }
  // add forced copy files
  for(auto f : m_forcedFiles) {
    string dst = GetProjectPath() + f->GetInstancePathName(EMPTY_STRING, 0, GetRteFolder());
    if(RteFsUtils::Exists(dst))
      continue;
    string src = f->GetOriginalAbsolutePath();
    if(!RteFsUtils::CopyCheckFile(src, dst, false)) {
      string str = "Error: cannot copy file\n";
      str += src;
      str += "\n to\n";
      str += dst;
      str += "\nOperation failed\n";
      GetCallback()->OutputErrMessage(str);
    }
  }
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
  for (auto [_, pi] : m_filteredPackages) {
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
  for (auto [id, pi] : m_filteredPackages) {
    if (commonId == RtePackage::CommonIdFromId(id)) {
      string version = RtePackage::VersionFromId(id);
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

RteGpdscInfo* RteProject::AddGpdscInfo(const string& gpdscFile, RtePackage* gpdscPack)
{
  string name = gpdscFile;
  if (!m_projectPath.empty()) {
    name = RteFsUtils::RelativePath(gpdscFile,m_projectPath);
  }

  RteGpdscInfo* gi = GetGpdscInfo(gpdscFile);
  if (!gi) {
    gi = new RteGpdscInfo(this, gpdscPack);
    gi->AddAttribute("name", name);
    m_gpdscInfos[gpdscFile] = gi;
    t_bGpdscListModified = true;
  } else {
    gi->SetGpdscPack(gpdscPack);
  }
  return gi;
}


RteGpdscInfo* RteProject::AddGpdscInfo(RteComponent* c, RteTarget* target)
{
  if (!c)
    return NULL;

  RteGenerator* gen = c->GetGenerator();
  if (!gen )
    return NULL; // nothing to insert

  // get absolute gpdsc file name
  string gpdsc = c->GetGpdscFile(target);
  if (gpdsc.empty())
    return NULL;

  RteGpdscInfo* gi = GetGpdscInfo(gpdsc);
  if (!gi) {
    gi = AddGpdscInfo(gpdsc, nullptr);
    error_code ec;
    if (ShouldUpdateRte() && !fs::exists(gpdsc, ec)) { // file not exists
    // create destination directory
      string dir = RteUtils::ExtractFilePath(gpdsc, true);
      // directory need to exist for modification watch
      while (!RteFsUtils::CreateDirectories(dir)) {
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
    if(!gi->GetGenerator()) { // gpdsc file can already contain generator
      gi->AddAttribute("generator", gen->GetID());
      gi->SetGenerator(gen);
    }
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

bool RteProject::HasGpdscPacks() const
{
  return !m_gpdscInfos.empty();
}

bool RteProject::HasMissingGpdscPacks() const
{
  if (m_gpdscInfos.empty())
    return false;
  for (auto [_, gi] : m_gpdscInfos) {
    if (!gi->GetGpdscPack())
      return true;
  }
  return false;
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
  for (auto [_, bi] : m_boardInfos) {
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
  for (auto it = m_targets.begin(); it != m_targets.end(); ++it) {
    CollectSettings(it->first);
  }
}


string RteProject::GetRteComponentsH(const string & targetName, const string & prefix) const
{
  return GetRteHeader(string("RTE_Components.h"), targetName, prefix);
}


string RteProject::GetRegionsHeader(const string& targetName, const string& prefix) const
{
  RteTarget* target = GetTarget(targetName);
  string regionsHeader = target ? target->GetRegionsHeader() : RteUtils::EMPTY_STRING;
  return prefix + regionsHeader;
}


string RteProject::GetRteHeader(const string& name, const string & targetName, const string & prefix) const
{
  string rteHeader = prefix;
  rteHeader += GetRteFolder() + "/";
  if (!targetName.empty()) {
    rteHeader += "_";
    rteHeader += WildCards::ToX(targetName);
    rteHeader += "/";
  }
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
  for (auto [_,ci] : m_components) {
    t->CollectComponentSettings(ci);
  }
  t->CollectClassDocs();

  // collect copied files and sources
  for (auto [_,fi] : m_files) {
    t->AddFileInstance(fi);
  }

  // add generator headers and include paths

  for (auto [_, gi] : m_gpdscInfos) {
    RteGenerator* gen = gi->GetGenerator();
    if (!gen)
      continue;
    RteFileContainer* projectFiles = gen->GetProjectFiles();
    if (projectFiles) {
      string comment = gen->GetName();
      comment += ":Common Sources";

      for (auto child : projectFiles->GetChildren()) {
        RteFile* f = dynamic_cast<RteFile*>(child);
        if (!f)
          continue;
        RteFile::Category cat = f->GetCategory();
        switch (cat) {
        case RteFile::Category::HEADER:
          t->AddFile(f->GetIncludeFileName(), cat, comment);
          if (f->GetScope() != RteFile::Scope::SCOPE_PRIVATE) {
            t->AddIncludePath(f->GetIncludePath(), f->GetLanguage());
          }
          break;
        case RteFile::Category::INCLUDE:
          // could we have situation that gpdsc describes private include paths directly in gpdsc outside component descriptions?
          if(f->GetScope() != RteFile::Scope::SCOPE_PRIVATE) {
            t->AddIncludePath(f->GetIncludePath(), f->GetLanguage());
          }
          break;
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
    t->AddIncludePath(RteUtils::ExtractFilePath(rteComponentsH, false), RteFile::Language::LANGUAGE_NONE);
    t->AddFile("RTE_Components.h", RteFile::Category::HEADER, "Component selection"); // add ".\RTE\_TargetName\RTE_Components.h" folder to all target includes
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
  for (auto [_, ci] : m_components) {
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
    RteItem componentAttributes(ci->GetAttributes()); // copy just attributes
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
  for (auto [_, pi] : m_filteredPackages) {
    CreateTargetModels(pi);
  }
  for (auto [id, ci] : m_components) {
    CreateTargetModels(ci);
  }

  PropagateFilteredPackagesToTargetModels();
}


void RteProject::CreateTargetModels(RteItemInstance* instance) {
  if (!instance)
    return;
  auto targetInfos = instance->GetTargetInfos();
  for (auto [targetName, ti] : targetInfos) {
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
    XmlItem targetAttributes(attributes);
    RteBoardInfo* boardInfo = target->GetBoardInfo();
    if (boardInfo) {
      targetAttributes.AddAttributes(boardInfo->GetAttributes(), false);
    }

    // add Brevision attribute if Bversion is specified
    if (!targetAttributes.HasAttribute("Brevision") && targetAttributes.HasAttribute("Bversion")) {
      targetAttributes.AddAttribute("Brevision", target->GetAttribute("Bversion"));
      targetAttributes.RemoveAttribute("Bversion");
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
  for (auto [_ , t] : m_targets) {
    delete t;
  }
  for (auto [_, m ] : m_targetModels) {
    delete m;
  }


  for (auto [_, ci] : m_components) {
    ci->ClearResolved();
  }

  for (auto [_, pi] : m_filteredPackages) {
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

  for (auto itp = m_filteredPackages.begin(); itp != m_filteredPackages.end(); ++itp) {
    RtePackageInstanceInfo* pi = itp->second;
    pi->AddTargetInfo(targetName, activeTarget);
  }

  for (auto [_, gi] : m_gpdscInfos) {
    gi->AddTargetInfo(targetName, activeTarget);
  }

  for (auto [_, bi] : m_boardInfos) {
    if (bi->IsUsedByTarget(activeTarget) && bi->ResolveBoard(targetName)) {
      bi->AddTargetInfo(targetName, activeTarget);
    }
  }

  for (auto [_, ci] : m_components) {
    ci->AddTargetInfo(targetName, activeTarget);
  }

  for (auto [_, fi] : m_files) {
    fi->AddTargetInfo(targetName, activeTarget);
  }
}


bool RteProject::RemoveTargetInfo(const string& targetName)
{
  bool changed = false;
  if (m_packFilterInfos->RemoveTargetInfo(targetName))
    changed = true;

  for (auto [_, pi] : m_filteredPackages) {
    if (pi->RemoveTargetInfo(targetName))
      changed = true;
  }

  for (auto [_, gi] : m_gpdscInfos) {
    if (gi->RemoveTargetInfo(targetName))
      changed = true;
  }

  for (auto [_, bi] : m_boardInfos) {
    if (bi->RemoveTargetInfo(targetName))
      changed = true;
  }

  for (auto [_, ci] : m_components) {
    if (ci->RemoveTargetInfo(targetName))
      changed = true;
  }

  for (auto [_, fi] : m_files) {
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

  for (auto [_, pi] : m_filteredPackages) {
    if (pi->RenameTargetInfo(oldName, newName))
      changed = true;
  }

  for (auto [_, gi] : m_gpdscInfos) {
    if (gi->RenameTargetInfo(oldName, newName))
      changed = true;
  }

  for (auto [_, bi] : m_boardInfos) {
    if (bi->RenameTargetInfo(oldName, newName)) {
      changed = true;
    }
  }

  for (auto [_, ci] : m_components) {
    if (ci->RenameTargetInfo(oldName, newName))
      changed = true;
  }

  for (auto [_, fi] : m_files) {
    if (fi->RenameTargetInfo(oldName, newName))
      changed = true;
  }
  return changed;
}

void RteProject::FilterComponents()
{
  RteTarget* activeTarget = GetActiveTarget();
  // iterate through full list selecting components that pass target filter
  for (auto it = m_targets.begin(); it != m_targets.end(); ++it) {
    if (it->second == activeTarget) // skip active target, evaluate it at the end
      continue;
    FilterComponents(it->second);
  }
  // apply active target filter as the last one
  FilterComponents(activeTarget);
}

void RteProject::ClearFilteredPackages()
{
  for (auto it = m_filteredPackages.begin(); it != m_filteredPackages.end(); ++it) {
    delete it->second;
  }
  m_packFilterInfos->Clear();
  m_filteredPackages.clear();
}


void RteProject::PropagateFilteredPackagesToTargetModels()
{
  for (auto it = m_targetModels.begin(); it != m_targetModels.end(); ++it) {
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
  set<string> fixedPacks;
  set<string> latestPacks;
  for (auto [id, pi] : m_filteredPackages) {
    if (!pi || !pi->IsFilteredByTarget(targetName))
      continue;

    if (pi->IsExcluded(targetName)) {
      continue;
    }

    VersionCmp::MatchMode mode = pi->GetVersionMatchMode(targetName);
    if (mode == VersionCmp::MatchMode::FIXED_VERSION) {
      fixedPacks.insert(id);
    } else {
      latestPacks.insert(pi->GetPackageID(false));
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
    for (auto [targetName, target] : m_targets) {
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
  for (auto [targetName, target] : m_targets) {
    const RtePackageFilter& filter = target->GetPackageFilter();
    if (filter.AreAllExcluded()) {
      RteInstanceTargetInfo* info = m_packFilterInfos->EnsureTargetInfo(targetName);
      info->SetExcluded(true);
    } else {
      m_packFilterInfos->RemoveTargetInfo(targetName);
    }

    const set<string>& latestPacks = filter.GetLatestPacks();
    for (auto& commonId : latestPacks) {
      auto itpi = m_filteredPackages.find(commonId);
      RtePackageInstanceInfo* pi = NULL;
      if (itpi != m_filteredPackages.end()) {
        pi = itpi->second;
      }
      if (!pi) {
        pi = new RtePackageInstanceInfo(this, commonId);
        m_filteredPackages[commonId] = pi;
        bModified = true;
      } else if (!pi->GetTargetInfo(targetName))
        bModified = true;

      pi->AddTargetInfo(targetName);
      if (pi->SetUseLatestVersion(true, targetName))
        bModified = true;
    }

    const set<string>& packs = filter.GetSelectedPackages();
    for (auto id : packs) {
      auto itpi = m_filteredPackages.find(id);
      RtePackageInstanceInfo* pi = NULL;
      if (itpi != m_filteredPackages.end()) {
        pi = itpi->second;
      }
      if (!pi) {
        pi = new RtePackageInstanceInfo(this, id);
        m_filteredPackages[id] = pi;
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
  for (it = m_targets.begin(); it != m_targets.end(); ++it) {
    RteTarget* target = it->second;
    target->ClearSelectedComponents();
  }
}

void RteProject::ClearUsedComponents()
{
  map<string, RteTarget*>::iterator it;
  for (it = m_targets.begin(); it != m_targets.end(); ++it) {
    RteTarget* target = it->second;
    target->ClearUsedComponents();
  }
}

void RteProject::PropagateActiveSelectionToAllTargets()
{
  RteTarget* activeTarget = GetActiveTarget();
  map<string, RteTarget*>::iterator it;
  for (it = m_targets.begin(); it != m_targets.end(); ++it) {
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
  for (auto it = tiMap.begin(); it != tiMap.end(); ++it) {
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
  for (auto itpi = m_filteredPackages.begin(); itpi != m_filteredPackages.end(); ++itpi) {
    CollectMissingPacks(itpi->second);
  }
  // collect missing pack from components
  for (auto itc = m_components.begin(); itc != m_components.end(); ++itc) {
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
  RteModel* rteModel = target->GetFilteredModel();

  target->ClearMissingPacks();

  // check if all required gpdsc files are available
  for (auto [_, gi] : m_gpdscInfos) {
    if (gi->GetGpdscPack()) {
      continue; // loaded
    }

    bValid = false;
    const string& generatorID = gi->GetGeneratorName();
    const string fileName = gi->GetAbsolutePath();
    error_code ec;
    const string msgBody = "Required input file from generator " + generatorID + ": '" + fileName + "'";
    if (!fs::exists(fileName, ec)) {
      string msg = "Error #545: ";
      msg += msgBody;
      msg += " is missing";
      m_errors.push_back(msg);
    } else {
      string msg = "Error #546: ";
      msg += msgBody;
      msg += "' is not loaded, errors by load";
      m_errors.push_back(msg);
    }
  }


  // check if all fixed packs are installed
  for (auto itpi = m_filteredPackages.begin(); itpi != m_filteredPackages.end(); ++itpi) {
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
  for (itc = m_components.begin(); itc != m_components.end(); ++itc) {
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
      RteItem::ConditionResult apiResult = c->GetConditionResult(target->GetDependencySolver());
      if (apiResult >= RteItem::FULFILLED) {
        continue;
      }
      string apiVer = c->GetApiVersionString();
      if (apiResult == RteItem::MISSING_API) {
        bValid = false;
        string msg = "Error #542: Component '";
        msg += c->GetFullDisplayName();
        msg += "': API version '";
        msg += apiVer;
        msg += "' or compatible is required.";
        msg += "API definition is missing (no pack ID is available)";
        m_errors.push_back(msg);
      } else if (apiResult == RteItem::MISSING_API_VERSION) {
        bValid = false;
        string msg = "Error #552: Component '";
        msg += c->GetFullDisplayName();
        msg += "': API version '";
        msg += apiVer;
        msg += "' or compatible is required.";
        m_errors.push_back(msg);
        list<RteApi*> availableApis = rteModel->GetAvailableApis(c->GetApiID(false));
        for(RteApi* availableApi :  availableApis) {
          string availableApiVer = availableApi->GetApiVersionString();
          msg = "   Version '";
          msg += availableApiVer;
          msg += "' is found in pack '";
          msg += availableApi->GetPackageID(true);
          if (availableApis.size() == 1) {
            msg += "', install ";
            msg += VersionCmp::Compare(apiVer, availableApiVer) < 0 ? "previous" : "next";
            msg += " pack version.";
          }
          m_errors.push_back(msg);
        }
      } else if (apiResult == RteItem::CONFLICT) {
        bValid = false;
        RteApi* api = c->GetApi(target, true);
        apiVer = api->GetVersionString();
        string msg = "Error #553: Component '";
        msg += c->GetFullDisplayName();
        if (api->IsExclusive()) {
          msg += "': conflicts with other components of the same API";
          msg += ": select only one component";
        } else {
          msg += "': uses API version '" + apiVer + "' that conflicts with other components of the same API";
          msg += ": select only components with compatible API versions";
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
  for (itc = m_components.begin(); itc != m_components.end(); ++itc) {
    RteComponentInstance* ci = itc->second;

    const RteInstanceTargetInfoMap& targetInfos = ci->GetTargetInfos();
    for (auto it = targetInfos.begin(); it != targetInfos.end(); ++it) {
      const string& targetName = it->first;
      RteTarget* target = GetTarget(targetName);
      if (target) {
        int count = it->second->GetInstanceCount();
        target->SetComponentUsed(ci, count);
      }
    }
  }
  for (auto it = m_targets.begin(); it != m_targets.end(); ++it) {
    RteTarget* target = it->second;
    target->CollectSelectedComponentAggregates();
    EvaluateComponentDependencies(target);
    target->CollectFilteredFiles();
  }
}

void RteProject::Construct()
{
  RteRootItem::Construct();
  for (auto child : GetChildren()) {
    RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(child);
    if (ci) {
      m_components[ci->GetID()] = ci;
    }
    RteFileInstance* fi = dynamic_cast<RteFileInstance*>(child);
    if (fi) {
      m_files[fi->GetID()] = fi;
    }
  }
  UpdateClasses();
}


RteItem* RteProject::AddChild(RteItem* child)
{
  if (child == m_packFilterInfos) {
    return child; // do not add to children
  }

  RtePackageInstanceInfo* pi = dynamic_cast<RtePackageInstanceInfo*>(child);
  if (pi) {
    m_filteredPackages[pi->GetPackageID(true)] = pi;
    // do not add to children
    return pi;
  }
  RteGpdscInfo* gi = dynamic_cast<RteGpdscInfo*>(child);
  if (gi) {
    m_gpdscInfos[gi->GetAbsolutePath()] = gi;
    // do not add to children
    return gi;
  }
  RteBoardInfo* bi = dynamic_cast<RteBoardInfo*>(child);
  if (bi) {
    m_boardInfos[bi->GetDisplayName()] = bi;
    // do not add to children
    return bi;
  }
  return RteRootItem::AddChild(child);
}

RteItem* RteProject::CreateItem(const std::string& tag)
{
  if (tag == "components" || tag == "apis" || tag == "files" || tag == "packages" || tag == "gpdscs" || tag == "boards") {
    return GetThis(); // factory will recursively call this function (processed in the next if clauses)
  } else if (tag == "component" || tag == "api") {
    return new RteComponentInstance(this);
  } else if (tag == "file") {
    return new RteFileInstance(this);
  } else if (tag == "package") {
   return new RtePackageInstanceInfo(this);
  } else if (tag == "gpdsc") {
    return new RteGpdscInfo(this);
  } else if (tag == "board") {
    return new RteBoardInfo(this);
  } else if (tag == "filter") {
    return m_packFilterInfos; // already exists
  }
  return RteRootItem::CreateItem(tag);
}


void RteProject::CreateXmlTreeElementContent(XMLTreeElement* parentElement) const
{
  XMLTreeElement *e = NULL;

  if (!m_filteredPackages.empty() || m_packFilterInfos->GetTargetCount() > 0) {
    e = new XMLTreeElement(parentElement);
    e->SetTag("packages");

    m_packFilterInfos->CreateXmlTreeElement(e);

    for (auto itp = m_filteredPackages.begin(); itp != m_filteredPackages.end(); ++itp) {
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
    for (auto itp = m_gpdscInfos.begin(); itp != m_gpdscInfos.end(); ++itp) {
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
    for (auto itb = m_boardInfos.begin(); itb != m_boardInfos.end(); ++itb) {
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
  for (itc = m_components.begin(); itc != m_components.end(); ++itc) {
    RteComponentInstance* ci = itc->second;
    if (ci && ci->IsApi())
      ci->CreateXmlTreeElement(e);
  }
  e = new XMLTreeElement(parentElement);
  e->SetTag("components");
  for (itc = m_components.begin(); itc != m_components.end(); ++itc) {
    RteComponentInstance* ci = itc->second;
    if (ci && ci->IsSelectable() && !ci->IsApi())
      ci->CreateXmlTreeElement(e);
  }
  e = new XMLTreeElement(parentElement);
  e->SetTag("files");
  map<string, RteFileInstance*>::const_iterator itf;
  for (itf = m_files.begin(); itf != m_files.end(); ++itf) {
    RteFileInstance* fi = itf->second;
    fi->CreateXmlTreeElement(e);
  }
}

void RteProject::GetUsedComponents(RteComponentMap & components, const string& targetName) const
{
  for (auto it = m_components.begin(); it != m_components.end(); ++it) {
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
  for (auto it = m_targets.begin(); it != m_targets.end(); ++it) {
    const string& targetName = it->first;
    GetUsedComponents(components, targetName);
  }
}

bool RteProject::IsComponentUsed(const string& aggregateId, const string& targetName) const
{
  for (auto it = m_components.begin(); it != m_components.end(); ++it) {
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

  for (auto it = m_components.begin(); it != m_components.end(); ++it) {
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

  for (auto [_, ci] : m_components) {
    if (!ci->IsUsedByTarget(targetName))
      continue;
    RtePackage* pack = ci->GetEffectivePackage(targetName);
    if (pack) {
      const string& id = pack->GetID();
      if (packs.find(id) == packs.end()) {
        packs[id] = pack;
      }
    }
  }
}

void RteProject::GetRequiredPacks(RtePackageMap& packs, const std::string& targetName) const
{
  RteTarget* t = GetTarget(targetName);
  if (!t)
    return;
  RtePackageMap usedPacks;
  GetUsedPacks(usedPacks, targetName); // get all used packs
  // add required packs
  RteModel* model = t->GetFilteredModel();
  for (auto [_, pack] : usedPacks) {
    pack->GetRequiredPacks(packs, model);
  }
}

bool RteProject::HasProjectGroup(const string& group) const
{
  map<string, RteTarget*>::const_iterator it;
  for (it = m_targets.begin(); it != m_targets.end(); ++it) {
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
  for (it = m_targets.begin(); it != m_targets.end(); ++it) {
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
  for (it = m_targets.begin(); it != m_targets.end(); ++it) {
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

void RteProject::CollectLicenseInfos(RteLicenseInfoCollection& licenseInfos) const
{
  //collect all components, APIs, DFPs and BSPs
  set<RteComponent*> components;
  set<RtePackage*> packs;
  for (auto [targetName, t] : GetTargets()) {
    CollectLicenseInfosForTarget(licenseInfos, targetName);
  }
}

void RteProject::CollectLicenseInfosForTarget(RteLicenseInfoCollection& licenseInfos, const std::string& targetName) const
{
  RteTarget* t = GetTarget(targetName); // returns active one if targetName is empty
  if (!t) {
    return; // no such target
  }
  set<RteComponent*> components;
  set<RtePackage*> packs;
  packs.insert(t->GetDevicePackage());
  packs.insert(t->GetBoardPackage());
  for (auto [_c, ci] : m_components) {
    RteComponent* c = ci->GetResolvedComponent(targetName);
    if (c) {
      components.insert(c);
    }
  }
  for (auto c : components) {
    licenseInfos.AddLicenseInfo(c);
  }
  for (auto p : packs) {
    licenseInfos.AddLicenseInfo(p);
  }
}
// End of RteProject.cpp
