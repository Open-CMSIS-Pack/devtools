/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteInstance.cpp
* @brief CMSIS RTE data model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteInstance.h"

#include "RteComponent.h"
#include "RteModel.h"
#include "RteProject.h"

#include "RteConstants.h"
#include "RteFsUtils.h"
#include "XMLTree.h"

using namespace std;

RteInstanceTargetInfo::RteInstanceTargetInfo(RteItem* parent) :
  RteItem(parent),
  m_bExcluded(false),
  m_bIncludeInLib(false),
  m_instanceCount(1),
  m_VersionMatchMode(VersionCmp::MatchMode::LATEST_VERSION)
{
};

RteInstanceTargetInfo::RteInstanceTargetInfo(RteInstanceTargetInfo* info) :
  RteItem(info->GetAttributes()),
  m_bExcluded(info->IsExcluded()),
  m_bIncludeInLib(info->IsIncludeInLib()),
  m_instanceCount(info->GetInstanceCount()),
  m_VersionMatchMode(info->GetVersionMatchMode())
{
  CopySettings(*info);
};


RteInstanceTargetInfo::RteInstanceTargetInfo(const map<string, string>& attributes) :
  RteItem(attributes),
  m_bExcluded(false),
  m_bIncludeInLib(false),
  m_instanceCount(1),
  m_VersionMatchMode(VersionCmp::MatchMode::LATEST_VERSION)
{
  ProcessAttributes();
}

void RteInstanceTargetInfo::ProcessAttributes()
{
  m_bExcluded = GetAttributeAsBool("excluded");
  m_bIncludeInLib = GetAttributeAsBool("includeInLib");

  m_instanceCount = GetAttributeAsInt("instances", 1);
  m_VersionMatchMode = VersionCmp::MatchModeFromString(GetAttribute("versionMatchMode"));
}

bool RteInstanceTargetInfo::SetExcluded(bool excluded)
{
  if (m_bExcluded != excluded) {
    m_bExcluded = excluded;
    if (m_bExcluded) {
      AddAttribute("excluded", "1");
    } else {
      RemoveAttribute("excluded");
    }
    return true;
  }
  return false;
}

bool RteInstanceTargetInfo::SetIncludeInLib(bool include)
{
  if (m_bIncludeInLib != include) {
    m_bIncludeInLib = include;
    if (m_bIncludeInLib) {
      AddAttribute("includeInLib", "1");
    } else {
      RemoveAttribute("includeInLib");
    }
    return true;
  }
  return false;
}


void RteInstanceTargetInfo::SetInstanceCount(int count)
{
  m_instanceCount = count;
  if (count != 1) {
    AddAttribute("instances", std::to_string(count));
  } else {
    RemoveAttribute("instances");
  }
}

bool RteInstanceTargetInfo::SetVersionMatchMode(VersionCmp::MatchMode mode)
{
  if (m_VersionMatchMode == mode)
    return false;
  m_VersionMatchMode = mode;
  if (mode == VersionCmp::MatchMode::LATEST_VERSION)
    RemoveAttribute("versionMatchMode");
  else {
    string sMode = VersionCmp::MatchModeToString(mode);
    SetAttribute("versionMatchMode", sMode.c_str());
  }
  return true;
}

RteItem* RteInstanceTargetInfo::GetOpt(RteOptType type)
{
  switch (type) {
  case MEMOPT:
    return &m_memOpt;
  case COPT:
    return &m_cOpt;
  case ASMOPT:
    return &m_asmOpt;
  default:
    break;
  }
  return NULL;
}

const RteItem* RteInstanceTargetInfo::GetOpt(RteOptType type) const
{
  switch (type) {
  case MEMOPT:
    return &m_memOpt;
  case COPT:
    return &m_cOpt;
  case ASMOPT:
    return &m_asmOpt;
  default:
    break;
  }
  return NULL;
}

bool RteInstanceTargetInfo::HasOptions() const
{
  return IsExcluded() || !m_memOpt.IsEmpty() || !m_cOpt.IsEmpty() || !m_asmOpt.IsEmpty();
}


void RteInstanceTargetInfo::CopySettings(const RteInstanceTargetInfo& other)
{
  SetVersionMatchMode(other.GetVersionMatchMode());
  SetExcluded(other.IsExcluded());
  SetIncludeInLib(other.IsIncludeInLib());
  m_memOpt.SetAttributes(other.GetMemOpt());
  m_cOpt.SetAttributes(other.GetCOpt());
  m_asmOpt.SetAttributes(other.GetAsmOpt());
}


XMLTreeElement* RteInstanceTargetInfo::CreateXmlTreeElement(XMLTreeElement* parentElement, bool bCreateContent) const
{
  XMLTreeElement* thisElement = new XMLTreeElement(parentElement);
  thisElement->SetTag("targetInfo");
  thisElement->SetAttributes(GetAttributes());
  if (bCreateContent) {
    // create content
    if (!m_memOpt.IsEmpty()) {
      XMLTreeElement* optElement = new XMLTreeElement(thisElement);
      optElement->SetTag("mem");
      optElement->CreateSimpleChildElements(m_memOpt.GetAttributes());
    }
    if (!m_cOpt.IsEmpty()) {
      XMLTreeElement* optElement = new XMLTreeElement(thisElement);
      optElement->SetTag("c");
      optElement->CreateSimpleChildElements(m_cOpt.GetAttributes());
    }

    if (!m_asmOpt.IsEmpty()) {
      XMLTreeElement* optElement = new XMLTreeElement(thisElement);
      optElement->SetTag("asm");
      optElement->CreateSimpleChildElements(m_asmOpt.GetAttributes());
    }
  }
  return thisElement;
}

void RteInstanceTargetInfo::Construct()
{
  if (GetTag() != "targetInfo") {
    SetTag("targetInfo");
  }
  if (!HasAttribute("name")) {
    AddAttribute("name", GetText());
  }
  ProcessAttributes();
  RteItem::Construct();
  set<RteItem*> toRemove;
  for (auto e : GetChildren()) {
    const string& tag = e->GetTag();
    if (tag == "mem") {
      map<string, string> attributes;
      e->GetSimpleChildElements(attributes);
      m_memOpt.SetAttributes(attributes);
      toRemove.insert(e);
    } else if (tag == "c") {
      map<string, string> attributes;
      e->GetSimpleChildElements(attributes);
      m_cOpt.SetAttributes(attributes);
      toRemove.insert(e);
    } else if (tag == "asm") {
      map<string, string> attributes;
      e->GetSimpleChildElements(attributes);
      m_asmOpt.SetAttributes(attributes);
      toRemove.insert(e);
    }
  }
  for (auto item : toRemove) {
    RemoveChild(item, true);
  }
}

RteItemInstance::RteItemInstance(RteItem* parent) :
  RteItem(parent),
  m_bRemoved(false)
{
}

RteItemInstance::~RteItemInstance()
{
  RteItemInstance::Clear();
}


void RteItemInstance::InitInstance(RteItem* item)
{
  if (!item)
    return;
  SetTag(item->GetTag());
  SetAttributes(item->GetAttributes());
}

void RteItemInstance::Clear()
{
  ClearTargets();
}

void RteItemInstance::ClearTargets()
{
  for (auto [_, ti] : m_targetInfos) {
    delete ti;
  }
  m_targetInfos.clear();
}

void RteItemInstance::PurgeTargets()
{
  for (auto it = m_targetInfos.begin(); it != m_targetInfos.end();) {
    auto itcurrent = it++;
    RteInstanceTargetInfo* ti = itcurrent->second;
    if (ti->GetInstanceCount() < 1) {
      m_targetInfos.erase(itcurrent);
      delete ti;
    }
  }
  if (m_targetInfos.empty())
    m_bRemoved = true;
}


void RteItemInstance::SetTargets(const RteInstanceTargetInfoMap& infos)
{
  for (auto [targetName, ti] : infos) {
    RteInstanceTargetInfo* copy = new RteInstanceTargetInfo(*(ti));
    m_targetInfos[targetName] = copy;
  }
}


bool RteItemInstance::IsUsedByTarget(const string& targetName) const
{
  if (IsRemoved())
    return false;
  RteInstanceTargetInfo* info = GetTargetInfo(targetName);
  return info && !info->IsExcluded();
}


bool RteItemInstance::IsFilteredByTarget(const string& targetName) const
{
  RteInstanceTargetInfo* info = GetTargetInfo(targetName);
  return info != NULL;
}


int RteItemInstance::GetInstanceCount(const string& targetName) const
{
  RteInstanceTargetInfo* info = GetTargetInfo(targetName);
  if (info)
    return info->GetInstanceCount();
  return 0;
}

const string& RteItemInstance::GetFirstTargetName() const
{
  auto it = m_targetInfos.begin();
  if (it != m_targetInfos.end()) {
    return it->first;
  }
  return EMPTY_STRING;
}

bool RteItemInstance::SetUseLatestVersion(bool bUseLatest, const string& targetName)
{
  VersionCmp::MatchMode mode = bUseLatest ? VersionCmp::MatchMode::LATEST_VERSION : VersionCmp::MatchMode::FIXED_VERSION;
  RteInstanceTargetInfo* ti = GetTargetInfo(targetName);
  if (ti) {
    return ti->SetVersionMatchMode(mode);
  }
  return false;
}

bool RteItemInstance::SetExcluded(bool excluded, const string& targetName)
{
  RteInstanceTargetInfo* ti = GetTargetInfo(targetName);
  if (ti) {
    return ti->SetExcluded(excluded);
  }
  return false;
}

bool RteItemInstance::IsExcluded(const string& targetName) const
{
  RteInstanceTargetInfo* ti = GetTargetInfo(targetName);
  if (ti) {
    return ti->IsExcluded();
  }
  return false;
}

bool RteItemInstance::SetIncludeInLib(bool include, const string& targetName)
{
  RteInstanceTargetInfo* ti = GetTargetInfo(targetName);
  if (ti) {
    return ti->SetIncludeInLib(include);
  }
  return false;
}

bool RteItemInstance::IsIncludeInLib(const string& targetName) const
{
  RteInstanceTargetInfo* ti = GetTargetInfo(targetName);
  if (ti) {
    return ti->IsIncludeInLib();
  }
  return false;
}


bool RteItemInstance::IsExcludedForAllTargets()
{
  for (auto [_, ti]  : m_targetInfos) {
    if (!ti->IsExcluded())
      return false;
  }
  return true;
}

void RteItemInstance::CopyTargetSettings(const RteInstanceTargetInfo& other, const string& targetName)
{
  RteInstanceTargetInfo* ti = GetTargetInfo(targetName);
  if (ti) {
    ti->CopySettings(other);
  }
}

RteTarget* RteItemInstance::GetTarget(const string& targetName) const
{
  RteProject* project = GetProject();
  if (project)
    return project->GetTarget(targetName.empty() ? GetFirstTargetName() : targetName);
  return NULL;
}

RteInstanceTargetInfo* RteItemInstance::GetTargetInfo(const string& targetName) const
{
  auto it = m_targetInfos.find(targetName);
  if (it != m_targetInfos.end()) {
    return it->second;
  }
  return nullptr;
}

RteInstanceTargetInfo* RteItemInstance::EnsureTargetInfo(const string& targetName)
{
  RteInstanceTargetInfo* info = GetTargetInfo(targetName);
  if (!info) {
    info = new RteInstanceTargetInfo(this);
    info->AddAttribute("name", targetName);
    m_targetInfos[targetName] = info;
  }
  return info;
}


VersionCmp::MatchMode RteItemInstance::GetVersionMatchMode(const string& targetName) const
{
  RteInstanceTargetInfo* info = GetTargetInfo(targetName);
  if (info)
    return info->GetVersionMatchMode();
  return VersionCmp::MatchMode::LATEST_VERSION;
}

RteInstanceTargetInfo* RteItemInstance::AddTargetInfo(const string& targetName, const string& copyFrom)
{
  RteInstanceTargetInfo* info = GetTargetInfo(targetName);
  if (info && targetName == copyFrom)
    return info;
  RteInstanceTargetInfo* src = GetTargetInfo(copyFrom);
  if (!src)
    return info; // source does not exist for this item => no need to copy it
  info = EnsureTargetInfo(targetName);
  if (src != info) { // conservative check
    info->CopySettings(*src);
    info->SetInstanceCount(src->GetInstanceCount());
  }
  return info;
}


RteInstanceTargetInfo* RteItemInstance::AddTargetInfo(const string& targetName)
{
  RteInstanceTargetInfo* info = EnsureTargetInfo(targetName);
  return info;
}

RteInstanceTargetInfo* RteItemInstance::AddTargetInfo(const string& targetName, const map<string, string>& attributes)
{
  RteInstanceTargetInfo* info = EnsureTargetInfo(targetName);
  info->AddAttributes(attributes, true);
  if (info->GetInstanceCount() < 1) {
    RemoveTargetInfo(targetName);
    info = NULL;
  }

  return info;
}

bool RteItemInstance::RemoveTargetInfo(const string& targetName, bool bDelete)
{
  auto it = m_targetInfos.find(targetName);
  if (it != m_targetInfos.end()) {
    if (bDelete) {
      delete it->second;
    }
    m_targetInfos.erase(it);
    return true;
  }
  return false;

}

bool RteItemInstance::RenameTargetInfo(const string& oldName, const string& newName)
{
  RteInstanceTargetInfo* info = GetTargetInfo(oldName);
  if (info) {
    RemoveTargetInfo(oldName, false);
    m_targetInfos[newName] = info;
    info->SetAttribute("name", newName.c_str());
    return true;
  }
  return false;
}

RtePackage* RteItemInstance::GetPackage() const
{
  RteModel* model = GetModel();
  RtePackage* p = 0;
  if (model) {
    string packageID = GetPackageID(true);
    if (!packageID.empty())
      p = model->GetPackage(packageID);
  }
  return p;
}


string RteItemInstance::GetPackageID(bool withVersion) const
{
  if (IsPackageInfo()) {
    return RtePackage::GetPackageIDfromAttributes(*this, withVersion);
  }
  return RtePackage::GetPackageIDfromAttributes(m_packageAttributes, withVersion);
}

const string& RteItemInstance::GetURL() const
{
  if (IsPackageInfo())
    return GetAttribute("url");
  return m_packageAttributes.GetAttribute("url");
}

const string& RteItemInstance::GetVendorString() const
{
  if (IsPackageInfo())
    return GetAttribute("vendor");
  return m_packageAttributes.GetVendorString();
}

const string& RteItemInstance::GetPackageVendorName() const
{
  if (IsPackageInfo())
    return GetVendorString();
  return m_packageAttributes.GetVendorString();
}


RteComponentInstance* RteItemInstance::GetComponentInstance(const string& targetName) const
{
  if (IsUsedByTarget(targetName)) {
    RteProject* project = GetProject();
    if (project && project->GetClasses()) {
      string aggregateID = GetComponentAggregateID();
      RteComponentInstanceAggregate* ai = project->GetClasses()->GetComponentInstanceAggregate(aggregateID);
      if (ai)
        return ai->GetComponentInstance(targetName);
    }
  }
  return NULL;
}

RtePackage* RteItemInstance::GetEffectivePackage(const string& targetName) const
{
  RteComponentInstance* ci = GetComponentInstance(targetName);
  if (ci && ci != this)
    return ci->GetEffectivePackage(targetName);

  RteModel* model = GetModel();
  RtePackage* p = 0;
  if (model) {
    RteProject * project = GetProject();
    string packId = GetPackageID(true);
    if (project) {
      packId = project->GetEffectivePackageID(packId, targetName);
    }
    if (!packId.empty()) {
      string commonId = RtePackage::CommonIdFromId(packId);
      if (commonId != packId)
        p = model->GetPackage(packId);
      else
        p = model->GetLatestPackage(commonId);
    }
  }
  return p;
}

string RteItemInstance::GetEffectivePackageID(const string& targetName) const
{
  RtePackage* pack = GetEffectivePackage(targetName);
  if (pack) {
    return pack->GetPackageID(true);
  }

  // check if to show pack version
  RteProject * project = GetProject();
  string packId = GetPackageID(true);
  if (project) {
    return project->GetEffectivePackageID(packId, targetName);
  }
  return packId;
}


RteComponent* RteItemInstance::GetComponent(const string& targetName) const
{
  RteComponent* c = 0;
  RteComponentInstance* ci = GetComponentInstance(targetName);

  if (ci) {
    c = ci->GetResolvedComponent(targetName);
  }
  return c;
}

RteComponent* RteItemInstance::GetResolvedComponent(const string& targetName) const
{
  if (IsFilteredByTarget(targetName)) {
    RteComponentInstance* ci = GetComponentInstance(targetName);
    if (ci)
      return ci->GetResolvedComponent(targetName);
  }
  return NULL;
}


void RteItemInstance::CreateXmlTreeElementContent(XMLTreeElement* parentElement) const
{
  XMLTreeElement* e = 0;
  if (!m_packageAttributes.IsEmpty()) {
    e = new XMLTreeElement(parentElement);
    e->SetTag("package");
    e->SetAttributes(m_packageAttributes.GetAttributes());
  }
  e = new XMLTreeElement(parentElement);
  e->SetTag("targetInfos");

  for (auto [_, ti] : m_targetInfos) {
    ti->CreateXmlTreeElement(e);
  }
}

RteItem* RteItemInstance::AddChild(RteItem* child)
{
  if (!child) {
    return child;
  }
  RteInstanceTargetInfo* ti = dynamic_cast<RteInstanceTargetInfo*>(child);
  if (ti) {
    const string& targetName = ti->GetName();
    RemoveTargetInfo(targetName, true); // ensure no duplicates
    m_targetInfos[targetName] = ti;
    return ti;
  }
  return RteItem::AddChild(child);
}

void RteItemInstance::Construct()
{
  RteItem::Construct();

  RteItem* packItem = GetFirstChild("package");
  if (packItem) {
    SetPackageAttributes(*packItem);
    RemoveChild(packItem, true); // we only need attributes, not the child
  }
}

RteItem* RteItemInstance::CreateItem(const std::string& tag)
{
  if (tag == "targets" || tag == "targetInfos") {
    return this; // we will add child elements directly below
  }
  if (tag == "target" || tag == "targetInfo") {
    return new RteInstanceTargetInfo(this);
  }
  return RteItem::CreateItem(tag);
}


RteFileInstance::RteFileInstance(RteItem* parent) :
  RteItemInstance(parent),
  m_instanceIndex(-1)
{
  m_tag = "file";
}

void RteFileInstance::Init(RteFile* f, const string& deviceName, int instanceIndex, const string& rteFolder)
{
  m_instanceName = f->GetInstancePathName(deviceName, instanceIndex, rteFolder);
  m_instanceIndex = instanceIndex;
  m_fileName = RteUtils::ExtractFileName(m_instanceName);
  m_bRemoved = false;
}

void RteFileInstance::Update(RteFile* f, bool bUpdateComponent)
{
  SetAttributes(f->GetAttributes());

  // get component properties
  RteComponent* c = f->GetComponent();
  if (!c) {
    return;
  }
  m_componentAttributes.SetAttributes(c->GetAttributes());

  if (c->IsApi())
    m_componentAttributes.SetTag("api");
  m_componentAttributes.RemoveAttribute("RTE_Components_h");

  // get package attributes
  RtePackage* package = c->GetPackage();
  if (package) {
    m_packageAttributes.SetAttributes(package->GetAttributes());
  }

  if (bUpdateComponent) {
    for (auto [targetName, ti] : m_targetInfos) {
      RteComponentInstance* ci = GetComponentInstance(targetName);
      if (ci) {
        ci->SetAttributes(m_componentAttributes);
        ci->SetPackageAttributes(m_packageAttributes);
      }
    }
  }
}

string RteFileInstance::GetIncludePath() const
{
  string instanceName = GetInstanceName();
  if (HasAttribute("path")) {
    string path = GetAttribute("path") + "/";
    string fileName = GetName();
    if (fileName.find(path) == 0) {
      fileName = fileName.substr(path.length());
      int nSegments = RteUtils::GetFileSegmentCount(instanceName) - RteUtils::GetFileSegmentCount(fileName);
      return RteUtils::ExtractFirstFileSegments(instanceName, nSegments);
    }
  }
  return RteUtils::ExtractFilePath(instanceName, false);
}


string RteFileInstance::GetIncludeFileName() const
{
  string instanceName = GetInstanceName();

  if (HasAttribute("path")) {
    // calculate number of section to remove from instance name
    string path = GetAttribute("path") + "/";
    string fileName = GetName();
    if (fileName.find(path) == 0) {
      fileName = fileName.substr(path.length());
      int nSegments = RteUtils::GetFileSegmentCount(fileName);
      return RteUtils::ExtractLastFileSegments(instanceName, nSegments);
    }
  }
  return RteUtils::ExtractFileName(instanceName);
}


bool RteFileInstance::IsConfig() const
{
  return GetAttribute("attr") == "config";
}

RteFile::Category RteFileInstance::GetCategory() const
{
  return RteFile::CategoryFromString(GetAttribute("category"));
}

RteFile::Scope RteFileInstance::GetScope() const
{
  return RteFile::ScopeFromString(GetAttribute("scope"));
}

RteFile::Language RteFileInstance::GetLanguage() const
{
  return RteFile::LanguageFromString(GetAttribute("language"));
}

int RteFileInstance::HasNewVersion(const string& targetName) const
{
  RteFile* f = GetFile(targetName);
  if (!f)
    return 0;
  int res = VersionCmp::Compare(f->GetVersionString(), GetVersionString());
  return res;
}

int RteFileInstance::HasNewVersion() const
{
  int newVersion = 0;
  for (auto [targetName, ti] : m_targetInfos) {
    int newVer = HasNewVersion(targetName);
    if (newVer > newVersion) {
      newVersion = newVer;
      if (newVersion > 2)
        break;
    }
  }
  return newVersion;
}

std::string RteFileInstance::GetInfoString(const std::string& targetName, const string& relativeTo) const
{
  string info;
  string absPath = GetAbsolutePath();
  if (!relativeTo.empty()) {
    info = RteFsUtils::RelativePath(absPath, relativeTo);
  } else {
    info = GetInstanceName();
  }

  const string& baseVersion = GetAttribute("version"); // explicitly check the file instance version
  if (!baseVersion.empty()) {
    info += RteConstants::PREFIX_CVERSION;
    info += baseVersion;
  }
  RteFile* f = GetFile(targetName);
  const string& updateVersion = f ? f->GetVersionString() : RteUtils::EMPTY_STRING;

  string state;
  if (!RteFsUtils::Exists(absPath)) {
    state = "not exist";
  } else if (!updateVersion.empty()) {
    if (VersionCmp::Compare(baseVersion, updateVersion) == 0) {
      state = "up to date";
    } else {
      state = "update";
      state += RteConstants::PREFIX_CVERSION;
      state += updateVersion;
    }
  }
  if (!state.empty()) {
    info += RteConstants::SPACE_STR;
    info += RteConstants::OBRACE_STR;
    info += state;
    info += RteConstants::CBRACE_STR;
  }
  info += " from ";
  info += GetComponentID(true);
  return info;
}

RteComponentInstance* RteFileInstance::GetComponentInstance(const string& targetName) const
{
  RteProject* project = GetProject();
  if (project && project->GetClasses()) {
    RteTarget* t = project->GetTarget(targetName);
    if (t != NULL) {
      RteComponentInstance* ci = t->GetComponentInstanceForFile(GetInstanceName());
      if (ci != NULL) {
        return ci;
      }
    }
  }
  return RteItemInstance::GetComponentInstance(targetName);
}


string RteFileInstance::GetComponentUniqueID() const
{
  return m_componentAttributes.GetComponentUniqueID();
}

string RteFileInstance::GetComponentID(bool withVersion) const
{
  return m_componentAttributes.GetComponentID(withVersion);
}


string RteFileInstance::GetComponentAggregateID() const
{
  return m_componentAttributes.GetComponentAggregateID();
}

string RteFileInstance::GetProjectGroupName() const
{
  return m_componentAttributes.GetProjectGroupName();
}

const string& RteFileInstance::GetVendorString() const
{
  const string& vendor = m_componentAttributes.GetVendorString();
  if (!vendor.empty())
    return vendor;
  return RteItemInstance::GetVendorString();
}

const string& RteFileInstance::GetCbundleName() const
{
  return m_componentAttributes.GetCbundleName();
}


string RteFileInstance::GetDisplayName() const
{
  string name = GetFileName();
  name += " ";
  name += GetFileComment();
  return name;
}

string RteFileInstance::GetFileComment() const
{
  string comment = "(";
  comment += m_componentAttributes.ConstructComponentDisplayName(false, false);
  comment += ")";
  return comment;
}

string RteFileInstance::GetHeaderComment() const
{
  return m_componentAttributes.GetPartialComponentID(false);
}

string RteFileInstance::GetAbsolutePath() const
{
  string s;
  if (IsConfig()) {
    RteProject* project = GetProject();
    if (project && !project->GetProjectPath().empty())
      s = project->GetProjectPath() + m_instanceName;
  } else {
    s = GetOriginalAbsolutePath();
  }
  return s;
}

RteFile* RteFileInstance::GetFile(const string& targetName) const
{
  RteTarget* t = GetTarget(targetName);
  if (t) {
    RteComponent* c = GetComponent(targetName);
    return t->GetFile(this, c, t->GetRteFolder(GetComponentInstance(targetName)));
  }
  return NULL;
}

const string& RteFileInstance::GetVersionString() const
{
  const string& ver = GetAttribute("version");
  if (!ver.empty())
    return ver;

  return GetComponentVersionString();
}

string RteFileInstance::Backup(bool bDeleteExisting)
{
  if (!IsConfig()) {
    return EMPTY_STRING; // nothing to do
  }
  string thisFile = GetAbsolutePath();
  string backupFile = RteFsUtils::BackupFile(thisFile, bDeleteExisting);
  // backup .base file if exists
  string baseFile = RteUtils::AppendFileBaseVersion(thisFile, GetVersionString());
  if (RteFsUtils::Exists(baseFile)) {
    // ensure the same backup number as this file
    string baseBackupFile = RteUtils::AppendFileBaseVersion(backupFile, GetVersionString());
    RteFsUtils::CopyCheckFile(baseFile, baseBackupFile, false);
  }
  return backupFile;
}

bool RteFileInstance::Copy(RteFile* f, bool bMerge)
{
  if (!IsConfig())
    return true; // nothing to do

  string src = f->GetOriginalAbsolutePath();
  string dst = GetAbsolutePath();
  if (src == dst) {
    return false; // should never happen!
  }
  // backup config file and its .base if available
  string bak = Backup(false); // before copy
  if (bak == RteUtils::ERROR_STRING) {
    return false;
  }

  if (!RteFsUtils::CopyMergeFile(src, dst, GetInstanceIndex(), false)) {
    return false;
  }

  if (bMerge) {
    RteProject* project = GetProject();
    if (project) {
      // we can use base file backup here, it should already exist
      string baseFile = RteUtils::AppendFileBaseVersion(bak, GetVersionString());
      project->MergeFiles(bak, dst, baseFile);
    }
  }
  return true;
}

void RteFileInstance::Construct()
{
  RteItemInstance::Construct();
  RteItem* instance = GetFirstChild("instance");
  if (instance) {
    m_instanceName = instance->GetText();
    m_fileName = RteUtils::ExtractFileName(m_instanceName);
    const string& sIndex = instance->GetAttribute("index");
    if (!sIndex.empty())
      m_instanceIndex = stoi(sIndex);
    const string& removed = instance->GetAttribute("removed");
    if (!removed.empty())
      m_bRemoved = stoi(removed) != 0;
  }

  RteItem* component = GetFirstChild("component");
  if (component) {
    m_componentAttributes.SetAttributes(component->GetAttributes());
  }
}

void RteFileInstance::CreateXmlTreeElementContent(XMLTreeElement* parentElement) const
{
  XMLTreeElement* e = new XMLTreeElement(parentElement);
  e->SetTag("instance");
  e->SetText(GetInstanceName().c_str());
  if (m_instanceIndex >= 0) {
    e->AddAttribute("index", std::to_string(m_instanceIndex));
  }
  if (m_bRemoved) {
    e->AddAttribute("removed", "1");
  }

  e = new XMLTreeElement(parentElement);
  e->SetTag("component");
  e->SetAttributes(m_componentAttributes.GetAttributes());

  RteItemInstance::CreateXmlTreeElementContent(parentElement);
}

////

RtePackageInstanceInfo::RtePackageInstanceInfo(RteItem* parent, const string& packId):
RteItemInstance(parent)
{
  SetPackId(packId);
};

void RtePackageInstanceInfo::SetPackId(const string& packId)
{
  m_ID = packId;
  m_commonID = RtePackage::CommonIdFromId(packId);
  AddAttribute("name", RtePackage::NameFromId(packId));
  AddAttribute("vendor", RtePackage::VendorFromId(packId));
  AddAttribute("version", RtePackage::VersionFromId(packId), false);
}

void RtePackageInstanceInfo::ProcessAttributes()
{
  m_ID = ConstructID();
};


void RtePackageInstanceInfo::ClearResolved()
{
  m_resolvedPacks.clear();
}


string RtePackageInstanceInfo::ConstructID()
{
  m_commonID = RtePackage::GetPackageIDfromAttributes(*this, false);
  return RtePackage::GetPackageIDfromAttributes(*this, true);
}


string RtePackageInstanceInfo::GetPackageID(bool withVersion) const
{
  if (GetID().empty()) {
    return RtePackage::GetPackageIDfromAttributes(*this, withVersion);
  }
  if (withVersion)
    return GetID();
  return GetCommonID();
}


RtePackage* RtePackageInstanceInfo::GetEffectivePackage(const string& targetName) const
{
  RtePackage* pack = GetResolvedPack(targetName);
  if (pack)
    return pack;
  return NULL;
}

RtePackage* RtePackageInstanceInfo::GetResolvedPack(const string& targetName) const
{
  auto it = m_resolvedPacks.find(targetName);
  if (it != m_resolvedPacks.end())
    return it->second;
  return NULL;
}

void RtePackageInstanceInfo::SetResolvedPack(RtePackage* pack, const string& targetName)
{
  m_resolvedPacks[targetName] = pack;
}

bool RtePackageInstanceInfo::ResolvePack()
{
  bool resolved = true;
  m_resolvedPacks.clear();
  for (auto [targetName, ti] : m_targetInfos) {
    if (!ResolvePack(targetName))
      resolved = false;
  }
  return resolved;
}


bool RtePackageInstanceInfo::ResolvePack(const string& targetName)
{
  if (!IsUsedByTarget(targetName))
    return true;
  VersionCmp::MatchMode mode = GetVersionMatchMode(targetName);
  if (mode == VersionCmp::MatchMode::EXCLUDED_VERSION)
    return true;
  RtePackage* pack = NULL;
  RteModel* model = GetModel();
  if (mode == VersionCmp::MatchMode::FIXED_VERSION) {
    pack = model->GetPackage(GetPackageID(true));
  } else {
    pack = model->GetLatestPackage(GetPackageID(false));
  }

  if (pack)
    SetResolvedPack(pack, targetName);

  return pack != NULL;
}


RteGpdscInfo::RteGpdscInfo(RteItem* parent, RtePackage* gpdscPack) :
  RteItemInstance(parent),
  m_gpdscPack(gpdscPack),
  m_generator(nullptr)
{
  SetGpdscPack(gpdscPack);
};

RteGpdscInfo::~RteGpdscInfo()
{
  if(m_gpdscPack)
    delete m_gpdscPack;
}

RteFileContainer* RteGpdscInfo::GetProjectFiles() const
{
  RteFileContainer* projectFiles = nullptr;
  RtePackage* pack = GetGpdscPack();
  if(pack) {
    // external generator case
    projectFiles = pack->GetGroups();
    if(projectFiles && HasAttribute("generator") ) {
      string name = ":" + GetAttribute("generator");
      projectFiles->AddAttribute("name", name);
    }
  }
  if(!projectFiles && GetGenerator()) {
    projectFiles = GetGenerator()->GetProjectFiles();
  }
  return projectFiles;
}

void RteGpdscInfo::SetGpdscPack(RtePackage* gpdscPack)
{
  if(m_gpdscPack == gpdscPack) {
    return;
  }
  if (m_gpdscPack) {
    if(m_generator->GetPackage() == m_gpdscPack) {
      m_generator = nullptr;
    }
    delete m_gpdscPack;
  }
  m_gpdscPack = gpdscPack;
  if (gpdscPack) {
    gpdscPack->Reparent(this, false); //  set parent chain, but not add as a child
    RteGenerator* gen = gpdscPack->GetFirstGenerator();
    if(!gen && HasAttribute("generator")) {
      // query global generator
      gen = GetCallback()->GetExternalGenerator(GetAttribute("generator")) ;
    }
    SetGenerator(gen);
  }
}

string RteGpdscInfo::GetAbsolutePath() const
{
  const string& name = GetName();
  RteProject* project = GetProject();
  if (name.length() < 2 || name[0] == '/' || name[1] == ':')
    return name;// absolute
  if (project && !project->GetProjectPath().empty()) {
    string abs = project->GetProjectPath() + name;
    return RteFsUtils::MakePathCanonical(abs);
  }
  return RteUtils::BackSlashesToSlashes(name);// absolute
}

RteBoardInfo::RteBoardInfo(RteItem* parent) :
  RteItemInstance(parent),
  m_board(nullptr)
{
}


RteBoardInfo::~RteBoardInfo()
{
  RteBoardInfo::Clear();
}

void RteBoardInfo::Clear()
{
  ClearResolved();
  RteItemInstance::Clear();
}

void RteBoardInfo::ClearResolved()
{
  m_board = nullptr;
}

void RteBoardInfo::Init(RteBoard* board)
{
  if (!board)
    return;
  m_board = board;
  AddAttribute("Bname", board->GetName());
  AddAttribute("Bversion", board->GetVersionString()); // for backward compatibility
  AddAttribute("Brevision", board->GetRevision());
  AddAttribute("Bvendor", board->GetVendorString());
  m_ID = GetDisplayName();

  // get package info
  RtePackage* package = board->GetPackage();
  if (package) {
    m_packageAttributes.SetAttributes(package->GetAttributes());
  }
}

void RteBoardInfo::InitInstance(RteItem* item) {
  if (!item)
    return;
  SetTag("board");
  AddAttribute("Bname", item->GetAttribute("Bname"));
  const string& revision = item->HasAttribute("Brevision") ? item->GetAttribute("Brevision") : item->GetAttribute("Bversion");
  AddAttribute("Bversion", revision); // for backward compatibility
  AddAttribute("Brevision", revision);
  AddAttribute("Bvendor", item->GetAttribute("Bvendor"));
  m_ID = GetDisplayName();
}

string RteBoardInfo::ConstructID()
{
  m_ID = GetDisplayName();
  return m_ID;
}

void RteBoardInfo::ResolveBoard()
{
  m_board = nullptr;
  for (auto [targetName, ti] : m_targetInfos) {
    RteBoard* board = ResolveBoard(targetName);
    if (board) {
      m_board = board;
      return;
    }
  }
}

RteBoard* RteBoardInfo::ResolveBoard(const string& targetName)
{
  RteProject* project = GetProject();
  if (project) {
    RteTarget* t = project->GetTarget(targetName);
    if (t) {
      return t->FindBoard(GetDisplayName());
    }
  }
  return nullptr;
}

const string& RteBoardInfo::GetRevision() const {
  if (HasAttribute("Brevision")) {
    return GetAttribute("Brevision");
  }
  return GetAttribute("Bversion");
}


string RteBoardInfo::GetDisplayName() const
{
  string name = GetName();
  const string& rev = GetVersionString();
  if (!rev.empty()) {
    name += " (" + rev + ")";
  }
  return name;
}

RtePackage* RteBoardInfo::GetPackage() const
{
  if (m_board)
    return m_board->GetPackage();
  return RteItemInstance::GetPackage();
}

string RteBoardInfo::GetPackageID(bool withVersion) const
{
  if (m_board)
    return m_board->GetPackageID(withVersion);
  return RteItemInstance::GetPackageID(withVersion);
}


RteItem::ConditionResult RteBoardInfo::GetResolveResult(const string& targetName) const
{
  if (!IsUsedByTarget(targetName))
    return IGNORED;
  if (m_board) {
    return FULFILLED;
  }
  RtePackage* pack = GetPackage();
  if (pack) {
    return UNAVAILABLE; // the pack is installed, but not available for current target
  }
  return MISSING;
}


RteComponentInstance::RteComponentInstance(RteItem* parent) :
  RteItemInstance(parent),
  m_copy(NULL)
{
}


RteComponentInstance::~RteComponentInstance()
{
  RteComponentInstance::Clear();
}

void RteComponentInstance::Clear()
{
  if (m_copy)
    delete m_copy;
  m_copy = 0;
  ClearResolved();
  RteItemInstance::Clear();
}

void RteComponentInstance::ClearResolved()
{
  m_resolvedComponents.clear();
  m_potentialComponents.clear();
}

void RteComponentInstance::Init(RteComponent* c)
{
  if (!c)
    return;
  InitInstance(c);
  RemoveAttribute("RTE_Components_h");
  RemoveAttribute("isDefaultVariant");
  m_ID = c->GetID();

  // use the original pack of bootstrap component if available
  if (c->HasAttribute("selectable") ) {
    RteItem* packInfo = c->GetFirstChild("package");
    if (packInfo) {
      m_packageAttributes.SetAttributes(packInfo->GetAttributes());
    }
    return;
  }
  // get package info from the component itself
  RtePackage* package = c->GetPackage();
  if (package) {
    m_packageAttributes.SetAttributes(package->GetAttributes());
  }
}

RteComponentInstance* RteComponentInstance::MakeCopy()
{
  if (m_copy)
    delete m_copy;
  m_copy = 0;
  m_copy = new RteComponentInstance(*this);
  m_copy->m_children.clear();
  m_copy->SetPackageAttributes(GetPackageAttributes());
  // copy target infos
  m_copy->SetTargets(m_targetInfos);
  return m_copy;
}


bool RteComponentInstance::Equals(RteComponentInstance* ci) const
{
  if (!ci)
    return false;

  if (IsTargetSpecific() != ci->IsTargetSpecific())
    return false;

  if (GetTargetCount() != ci->GetTargetCount())
    return false;

  if (GetCvariantName() != ci->GetCvariantName())
    return false;

  if (GetVersionString() != ci->GetVersionString())
    return false;

  if (!DeviceVendor::Match(GetVendorString(), ci->GetVendorString()))
    return false;

  if (GetCclassName() != ci->GetCclassName())
    return false;

  if (GetCgroupName() != ci->GetCgroupName())
    return false;

  if (IsRemoved() != ci->IsRemoved())
    return false;

  for (auto [targetName, ti] : m_targetInfos) {
    RteInstanceTargetInfo* tiThis = ti;
    RteInstanceTargetInfo* tiThat = ci->GetTargetInfo(targetName);
    if (!tiThat)
      return false;

    if (tiThis->GetVersionMatchMode() != tiThat->GetVersionMatchMode())
      return false;

    if (tiThis->IsExcluded() != tiThat->IsExcluded())
      return false;

    if (tiThis->IsIncludeInLib() != tiThat->IsIncludeInLib())
      return false;

    if (tiThis->GetInstanceCount() != tiThat->GetInstanceCount())
      return false;

    if (tiThis->GetMemOpt().Compare(tiThat->GetMemOpt()) == false)
      return false;
    if (tiThis->GetCOpt().Compare(tiThat->GetCOpt()) == false)
      return false;
    if (tiThis->GetAsmOpt().Compare(tiThat->GetAsmOpt()) == false)
      return false;

  }

  return true;
}

bool RteComponentInstance::IsModified() const
{
  return !Equals(m_copy);
}


bool RteComponentInstance::IsVersionMatchFixed() const
{
  return GetAttribute("versionMatchMode") == "fixed";
}

bool RteComponentInstance::IsVersionMatchLatest() const
{
  return GetAttribute("versionMatchMode") == "latest";
}

bool RteComponentInstance::HasAggregateID(const string& aggregateId) const
{
  string id = GetComponentAggregateID();
  if (id == aggregateId)
    return true;
  // check resolved components : they could have resolved components with bundles
  for (auto [_, c] : m_resolvedComponents) {
    if (!c)
      continue;
    id = c->GetComponentAggregateID();
    if (id == aggregateId)
      return true;
  }
  return false;
}

string RteComponentInstance::ConstructID()
{
  m_ID.clear();
  // ensure Cvendor for components
  if (IsApi()) {
    if (!GetAttribute("Cvendor").empty())
      RemoveAttribute("Cvendor");
  } else if (GetAttribute("Cvendor").empty()) {
    AddAttribute("Cvendor", m_packageAttributes.GetAttribute("vendor"));
  }
  m_ID = GetComponentUniqueID();
  return m_ID;
}


const string& RteComponentInstance::GetVendorString() const
{
  if (IsApi())
    return EMPTY_STRING;
  const string& vendor = GetAttribute("Cvendor");
  if (!vendor.empty())
    return vendor;
  return RteItemInstance::GetVendorString();
}

void RteComponentInstance::SetRemoved(bool removed)
{
  RteItemInstance::SetRemoved(removed);
  if (removed) {
    ClearResolved();
    for (auto [targetName, ti] : m_targetInfos) {
      ti->SetInstanceCount(0);
    }
  }
}

bool RteComponentInstance::IsRemoved() const
{
  if (RteItemInstance::IsRemoved())
    return true;
  if (m_targetInfos.empty())
    return true;
  for (auto [targetName, ti] : m_targetInfos) {
    if (ti->GetInstanceCount() != 0)
      return false;
  }
  return true;
}


bool RteComponentInstance::IsTargetSpecific() const
{
  const string& s = GetAttribute("isTargetSpecific");
  return s == "1" || s == "true";
}

bool RteComponentInstance::SetTargetSpecific(bool bSet)
{
  if (IsTargetSpecific() == bSet)
    return false;
  if (bSet)
    SetAttribute("isTargetSpecific", "1");
  else
    RemoveAttribute("isTargetSpecific");
  return true;
}

bool RteComponentInstance::SetVariant(const string& variant)
{
  return AddAttribute("Cvariant", variant, false);
}

bool RteComponentInstance::SetVersion(const string& version)
{
  return AddAttribute("Cversion", version, false);
}

RteItem* RteComponentInstance::GetEffectiveItem(const string& targetName) const
{
  RteComponent* c = GetResolvedComponent(targetName);
  if (c)
    return c;
  return const_cast<RteComponentInstance*>(this);
}


RteComponent* RteComponentInstance::GetResolvedComponent(const string& targetName) const
{
  auto it = m_resolvedComponents.find(targetName);
  if (it != m_resolvedComponents.end())
    return it->second;
  return NULL;
}

RteComponent* RteComponentInstance::GetPotentialComponent(const string& targetName) const
{
  auto it = m_potentialComponents.find(targetName);
  if (it != m_potentialComponents.end())
    return it->second;
  return NULL;
}

RtePackage* RteComponentInstance::GetEffectivePackage(const string& targetName) const
{
  RteComponent* c = GetResolvedComponent(targetName);
  if (!c)
    c = GetPotentialComponent(targetName);
  if (c)
    return c->GetPackage();
  return RteItemInstance::GetEffectivePackage(targetName);
}

string RteComponentInstance::GetEffectiveDisplayName(const string& targetName) const
{
  RteComponent* c = GetResolvedComponent(targetName);
  if (!c)
    c = GetPotentialComponent(targetName);
  if (c)
    return c->GetFullDisplayName();
  return GetFullDisplayName();
}



void RteComponentInstance::SetResolvedComponent(RteComponent*c, const string& targetName)
{
  m_resolvedComponents[targetName] = c;
}

void RteComponentInstance::SetPotentialComponent(RteComponent*c, const string& targetName)
{
  m_potentialComponents[targetName] = c;
}


string RteComponentInstance::GetDocFile() const
{
  RteProject* project = GetProject();
  if (project) {
    RteComponent* c = GetResolvedComponent(project->GetActiveTargetName());
    if (c)
      return c->GetDocFile();
  }
  return EMPTY_STRING;
}

RteItem::ConditionResult RteComponentInstance::GetResolveResult(const string& targetName) const
{
  ConditionResult res = FULFILLED;
  RteComponent* c = GetResolvedComponent(targetName);
  if (!c) {
    RtePackage* pack = GetEffectivePackage(targetName);
    if (pack) {
      if (GetPotentialComponent(targetName))
        return UNAVAILABLE_PACK; // a component is actually installed, but pack is not selected
      else
        return UNAVAILABLE; // a component is actually installed, but not available for current target
    }
    return MISSING;
  } else if (res > SELECTABLE && !c->IsApi() && c->GetVersionString() != GetVersionString()) {
    res = SELECTABLE;
  }
  return res;
}


void RteComponentInstance::ResolveComponent()
{
  ClearResolved();
  for (auto [targetName, ti] : m_targetInfos) {
    ResolveComponent(targetName);
  }
}

RteComponent* RteComponentInstance::ResolveComponent(const string& targetName)
{
  RteComponent* c = NULL;
  RteComponent* potential = NULL;
  RteProject* project = GetProject();
  if (project) {
    RteTarget* t = project->GetTarget(targetName);
    if (t) {
      c = t->ResolveComponent(this);
      if (!c) {
        potential = t->GetPotentialComponent(this);
      }
    }
  }
  if (c)
    m_resolvedComponents[targetName] = c;
  if (potential)
    m_potentialComponents[targetName] = potential;
  return c;
}



string RteComponentInstance::GetFullDisplayName() const
{
  return GetComponentID(true);
}

string RteComponentInstance::GetDisplayName() const
{
  return GetComponentAggregateID();;
}

string RteComponentInstance::GetShortDisplayName() const
{
  string name = ConstructComponentDisplayName();
  if (IsApi()) {
    name += " (API)";
  }
  return name;
}

RteComponentInstance* RteComponentInstance::GetApiInstance() const
{
  RteProject* mi = GetProject();
  if (mi)
    return mi->GetApiInstance(GetAttributes());
  return NULL;
}

const string& RteComponentInstance::GetVersionString() const
{
  if (IsApi())
    return GetApiVersionString();
  return RteItemInstance::GetVersionString();
}



RteComponentInstanceAggregate::RteComponentInstanceAggregate(RteItem* parent) :
  RteItem(parent),
  m_bHasMaxInstances(false)
{
}

RteComponentInstanceAggregate::~RteComponentInstanceAggregate()
{
  RteComponentInstanceAggregate::Clear();
}

void RteComponentInstanceAggregate::Clear()
{
  m_children.clear(); // do not delete, we do not own it
}


RteComponentInstance* RteComponentInstanceAggregate::GetComponentInstance(const string& targetName) const
{
  for (auto child : m_children) {
    RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(child);
    if (ci && ci->IsFilteredByTarget(targetName))
      return ci;
  }
  return NULL;
}

RteComponentAggregate* RteComponentInstanceAggregate::GetComponentAggregate(const string& targetName) const
{
  RteProject* project = GetProject();
  if (project)
  {
    RteTarget* t = project->GetTarget(targetName);
    if (t)
      return t->GetComponentAggregate(m_ID);
  }
  return NULL;
}

bool RteComponentInstanceAggregate::IsModified() const
{
  for (auto child : m_children) {
    RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(child);
    if (ci && ci->IsModified())
      return true;
  }
  return false;
}

RteComponentInstance* RteComponentInstanceAggregate::GetModifiedInstance() const
{
  for (auto child : m_children) {
    RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(child);
    if (ci && ci->IsModified())
      return ci;
  }
  return NULL;
}

bool RteComponentInstanceAggregate::IsUnresolved(const string& targetName, bool bCopy) const
{
  RteComponentInstance* ci = GetComponentInstance(targetName);
  if (ci && bCopy)
    ci = ci->GetCopy();
  if (ci && ci->IsUsedByTarget(targetName) && !ci->IsRemoved()) {
    RteComponent* c = ci->GetResolvedComponent(targetName);
    if (c == NULL)
      return true;
    RteTarget* t = GetProject()->GetTarget(targetName);
    if (t && c->IsApiAvailable(t) < FULFILLED)
      return true;
  }
  return false; // there is nothing to resolve
}


bool RteComponentInstanceAggregate::IsFilteredByTarget(const string& targetName) const
{
  RteComponentInstance* ci = GetComponentInstance(targetName);
  if (ci) {
    return ci->IsFilteredByTarget(targetName);
  }
  return false;
}

bool RteComponentInstanceAggregate::IsUsedByTarget(const string& targetName) const
{
  RteComponentInstance* ci = GetComponentInstance(targetName);
  if (ci) {
    return ci->IsUsedByTarget(targetName);
  }
  return false;
}

bool RteComponentInstanceAggregate::IsExcluded(const string& targetName) const
{
  RteComponentInstance* ci = GetComponentInstance(targetName);
  if (ci) {
    return ci->IsExcluded(targetName);
  }
  return false;
}

bool RteComponentInstanceAggregate::IsTargetSpecific() const
{
  if (GetChildCount() > 1)
    return true;
  for (auto child : m_children) {
    RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(child);
    if (ci && ci->IsTargetSpecific())
      return true;
  }
  return false;
}


bool RteComponentInstanceAggregate::AllowsCommonSettings() const
{
  // default returns true
  return true;
}


void RteComponentInstanceAggregate::AddComponentInstance(RteComponentInstance* ci)
{
  if (!ci)
    return;
  if (GetChildCount() == 0) {
    m_ID = ci->GetComponentAggregateID();
    m_fullDisplayName = ci->GetComponentAggregateID();
    ClearAttributes();
    AddAttribute("Cclass", ci->GetCclassName());
    AddAttribute("Cgroup", ci->GetCgroupName());
    AddAttribute("Csub", ci->GetCsubName());
    AddAttribute("Cvendor", ci->GetVendorName());
  }
  // ensure copy of the instance for using in editing operations
  ci->MakeCopy(); // the instance maintains its copy internally
  AddItem(ci); // add the instance, not copy
  if (ci->HasMaxInstances())
    m_bHasMaxInstances = true;
}

string RteComponentInstanceAggregate::GetDisplayName() const
{
  string id;
  if (!GetCsubName().empty()) {
    id = GetCsubName();
  } else {
    id = GetCgroupName();
  };
  return id;
}

bool RteComponentInstanceAggregate::HasAggregateID(const string& aggregateId) const
{
  if (m_ID == aggregateId)
    return true;
  for (auto child : m_children) {
    RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(child);
    if (ci && ci->HasAggregateID(aggregateId))
      return true;
  }
  return false;
}

bool RteComponentInstanceAggregate::HasComponentInstance(RteComponentInstance* ci) const
{
  for (auto child : m_children) {
    if (ci == child)
      return true;
  }
  return false;
}


RteComponentInstanceGroup::RteComponentInstanceGroup(RteItem* parent) :
  RteItem(parent),
  m_apiInstance(0)
{
}

RteComponentInstanceGroup::~RteComponentInstanceGroup()
{
  RteComponentInstanceGroup::Clear();
}

void RteComponentInstanceGroup::Clear()
{
  for (auto [_, g] : m_groups)
  {
    delete g;
  }
  m_apiInstance = 0;
  m_groups.clear();
  RteItem::Clear();
}

bool RteComponentInstanceGroup::HasSingleAggregate() const
{
  return GetSingleComponentInstanceAggregate() != NULL;
}


RteComponentInstanceAggregate* RteComponentInstanceGroup::GetSingleComponentInstanceAggregate() const
{
  if (m_groups.empty() && GetChildCount() == 1) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(*m_children.begin());
    if (a && a->GetCsubName().empty())
      return a; // the single aggregate has the same name as the group
  }
  return NULL;
}


RteComponentInstanceAggregate* RteComponentInstanceGroup::GetComponentInstanceAggregate(const string& id) const
{
  for (auto child : m_children) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(child);
    if (!a) {
      continue; // should not happen
    }
    if (a->HasAggregateID(id)) {
      return a;
    }
  }
  for (auto [_, g] :  m_groups) {
    RteComponentInstanceAggregate* a = g->GetComponentInstanceAggregate(id);
    if (a)
      return a;
  }
  return NULL;
}

RteComponentInstanceAggregate* RteComponentInstanceGroup::GetComponentInstanceAggregate(RteComponentInstance* ci) const
{
  for (auto child : m_children) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(child);
    if (a && a->HasComponentInstance(ci)) {
      return a;
    }
  }
  for (auto [_, g] : m_groups) {
    RteComponentInstanceAggregate* a = g->GetComponentInstanceAggregate(ci);
    if (a)
      return a;
  }
  return NULL;
}


RteComponentInstanceGroup* RteComponentInstanceGroup::GetComponentInstanceGroup(RteComponentInstance* ci) const
{
  if (GetApiInstance() == ci)
    return const_cast<RteComponentInstanceGroup*>(this);

  for (auto child : m_children) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(child);
    if (a && a->HasComponentInstance(ci)) {
      return const_cast<RteComponentInstanceGroup*>(this);
    }
  }
  for (auto [_, g] : m_groups) {
    RteComponentInstanceGroup* cig = g->GetComponentInstanceGroup(ci);
    if (cig)
      return cig;
  }
  return NULL;
}


string RteComponentInstanceGroup::GetDisplayName() const
{
  string id = GetName();
  if (m_apiInstance)
    id += " (API)";
  return id;
}


RteComponentInstanceGroup* RteComponentInstanceGroup::GetGroup(const string& name) const
{
  map<string, RteComponentInstanceGroup*>::const_iterator it = m_groups.find(name);
  if (it != m_groups.end())
    return it->second;
  return NULL;

}


RteComponentInstanceGroup* RteComponentInstanceGroup::EnsureGroup(const string& name)
{
  map<string, RteComponentInstanceGroup*>::iterator it = m_groups.find(name);
  if (it != m_groups.end())
    return it->second;
  RteComponentInstanceGroup* group = new RteComponentInstanceGroup(this);
  group->SetTag(name);
  m_groups[name] = group;

  return group;
}

void RteComponentInstanceGroup::AddComponentInstance(RteComponentInstance* ci)
{
  if (ci->IsApi()) {
    m_apiInstance = ci;
    return;
  }
  string aggregateId = ci->GetComponentAggregateID();
  RteComponentInstanceAggregate* a = GetComponentInstanceAggregate(aggregateId);
  if (!a) {
    a = new RteComponentInstanceAggregate(this);
    AddItem(a);
  }
  a->AddComponentInstance(ci);
}

void RteComponentInstanceGroup::GetInstanceAggregates(set<RteComponentInstanceAggregate*>& aggregates) const
{
  for (auto child : m_children) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(child);
    aggregates.insert(a);
  }

  for (auto [_, g] : m_groups) {
    g->GetInstanceAggregates(aggregates);
  }
}


void RteComponentInstanceGroup::GetModifiedInstanceAggregates(set<RteComponentInstanceAggregate*>& modifiedAggregates) const
{
  for (auto child : m_children) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(child);
    if (a && a->IsModified())
      modifiedAggregates.insert(a);
  }

  for (auto [_, g] : m_groups) {
    g->GetModifiedInstanceAggregates(modifiedAggregates);
  }
}


bool RteComponentInstanceGroup::HasUnresolvedComponents(const string& targetName, bool bCopy) const
{
  if (m_apiInstance && !m_apiInstance->GetResolvedComponent(targetName))
    return true;

  for (auto child : m_children) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(child);
    if (a && a->IsUnresolved(targetName, bCopy))
      return true;
  }

  for (auto [_, g] : m_groups) {
    if (g->HasUnresolvedComponents(targetName, bCopy))
      return true;
  }
  return false;
}

RteItem::ConditionResult RteComponentInstanceGroup::GetConditionResult(RteConditionContext* context) const
{
  const RteTarget* t = context->GetTarget();
  const string& targetName = t->GetName();
  RteItem::ConditionResult result = RteItem::ConditionResult::IGNORED;
  for (auto child : m_children) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(child);
    RteComponentAggregate* ca = a? a->GetComponentAggregate(targetName): nullptr;
    if (!ca) {
      continue;
    }
    auto res = ca->GetConditionResult(context);
    if (result > res) {
      result = res;
    }
  }
  for (auto [_, g] : m_groups) {
    auto res = g->GetConditionResult(context);
    if (result > res) {
      result = res;
    }
  }
  return result;
}

bool RteComponentInstanceGroup::IsUsedByTarget(const string& targetName) const
{
  for (auto child : m_children) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(child);
    if (a && a->IsUsedByTarget(targetName))
      return true;
  }

  for (auto [_, g] : m_groups) {
    if (g->IsUsedByTarget(targetName))
      return true;
  }
  return false;
}

// End of RteInstance.cpp
