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

#include "RteFsUtils.h"
#include "XMLTree.h"

using namespace std;

RteInstanceTargetInfo::RteInstanceTargetInfo() :
  RteAttributes(),
  m_bExcluded(false),
  m_bIncludeInLib(false),
  m_instanceCount(1),
  m_VersionMatchMode(VersionCmp::LATEST_VERSION)
{
};

RteInstanceTargetInfo::RteInstanceTargetInfo(RteInstanceTargetInfo* info) :
  RteAttributes(info->GetAttributes()),
  m_bExcluded(info->IsExcluded()),
  m_bIncludeInLib(info->IsIncludeInLib()),
  m_instanceCount(info->GetInstanceCount()),
  m_VersionMatchMode(info->GetVersionMatchMode())
{
  CopySettings(*info);
};


RteInstanceTargetInfo::RteInstanceTargetInfo(const map<string, string>& attributes) :
  RteAttributes(attributes),
  m_bExcluded(false),
  m_bIncludeInLib(false),
  m_instanceCount(1),
  m_VersionMatchMode(VersionCmp::LATEST_VERSION)
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
  if (mode == VersionCmp::LATEST_VERSION)
    RemoveAttribute("versionMatchMode");
  else {
    string sMode = VersionCmp::MatchModeToString(mode);
    SetAttribute("versionMatchMode", sMode.c_str());
  }
  return true;
}

RteAttributes* RteInstanceTargetInfo::GetOpt(RteOptType type)
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

const RteAttributes* RteInstanceTargetInfo::GetOpt(RteOptType type) const
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
  m_memOpt = other.GetMemOpt();
  m_cOpt = other.GetCOpt();
  m_asmOpt = other.GetAsmOpt();
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


bool RteInstanceTargetInfo::ProcessXmlChildren(XMLTreeElement* xmlElement)
{
  if (!xmlElement)
    return false;
  // process children
  const list<XMLTreeElement*>& children = xmlElement->GetChildren();
  for (auto it = children.begin(); it != children.end(); it++) {
    XMLTreeElement* e = *it;
    const string& tag = e->GetTag();
    if (tag == "mem") {
      map<string, string> attributes;
      e->GetSimpleChildElements(attributes);
      m_memOpt.SetAttributes(attributes);
    } else if (tag == "c") {
      map<string, string> attributes;
      e->GetSimpleChildElements(attributes);
      m_cOpt.SetAttributes(attributes);
    } else if (tag == "asm") {
      map<string, string> attributes;
      e->GetSimpleChildElements(attributes);
      m_asmOpt.SetAttributes(attributes);
    }
  }
  return true;
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
  for (auto it = m_targetInfos.begin(); it != m_targetInfos.end(); it++) {
    delete it->second;
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
  for (auto it = infos.begin(); it != infos.end(); it++) {
    RteInstanceTargetInfo* copy = new RteInstanceTargetInfo(*(it->second));
    m_targetInfos[it->first] = copy;
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
  VersionCmp::MatchMode mode = bUseLatest ? VersionCmp::LATEST_VERSION : VersionCmp::FIXED_VERSION;
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
  for (auto it = m_targetInfos.begin(); it != m_targetInfos.end(); it++) {
    RteInstanceTargetInfo* ti = it->second;
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
  if (it != m_targetInfos.end())
    return it->second;
  return NULL;
}

RteInstanceTargetInfo* RteItemInstance::EnsureTargetInfo(const string& targetName)
{
  RteInstanceTargetInfo* info = GetTargetInfo(targetName);
  if (!info) {
    info = new RteInstanceTargetInfo();
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
  return VersionCmp::LATEST_VERSION;
}

RteInstanceTargetInfo* RteItemInstance::AddTargetInfo(const string& targetName, const string& copyFrom)
{
  RteInstanceTargetInfo* info = GetTargetInfo(targetName);
  if (targetName == copyFrom)
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
    if (bDelete)
      delete it->second;
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
  if (IsPackageInfo())
    return RteAttributes::GetPackageID(withVersion);

  return m_packageAttributes.GetPackageID(withVersion);
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

  for (auto it = m_targetInfos.begin(); it != m_targetInfos.end(); it++) {
    RteInstanceTargetInfo* ti = it->second;
    ti->CreateXmlTreeElement(e);
  }
}


bool RteItemInstance::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  const string& tag = xmlElement->GetTag();
  if (tag == "targets" || tag == "targetInfos") {
    return ProcessXmlChildren(xmlElement); // will recursively call this function (processed in the next if clauses)
  } else if (tag == "target") {
    const string& targetName = xmlElement->GetText();
    AddTargetInfo(targetName, xmlElement->GetAttributes());
    return true;
  } else if (tag == "targetInfo") {
    const string& targetName = xmlElement->GetAttribute("name");
    RteInstanceTargetInfo* ti = AddTargetInfo(targetName, xmlElement->GetAttributes());
    ti->ProcessXmlChildren(xmlElement);
    return true;
  } else if (tag == "package") {
    m_packageAttributes.SetAttributes(xmlElement->GetAttributes());
    return ProcessXmlChildren(xmlElement); // will recursively call this function
  }
  return RteItem::ProcessXmlElement(xmlElement);
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
  m_componentAttributes.SetAttributes(c->GetAttributes());

  if (c->IsApi())
    m_componentAttributes.SetTag("api");
  m_componentAttributes.RemoveAttribute("RTE_Components_h");

  // get package attributes
  RtePackage* package = c->GetPackage();
  m_packageAttributes.SetAttributes(package->GetAttributes());

  if (bUpdateComponent) {
    for (auto it = m_targetInfos.begin(); it != m_targetInfos.end(); it++) {
      RteComponentInstance* ci = GetComponentInstance(it->first);
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
  for (auto it = m_targetInfos.begin(); it != m_targetInfos.end(); it++) {
    int newVer = HasNewVersion(it->first);
    if (newVer > newVersion) {
      newVersion = newVer;
      if (newVersion > 2)
        break;
    }
  }
  return newVersion;
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


string RteFileInstance::GetComponentUniqueID(bool withVersion) const
{
  string id = m_componentAttributes.GetComponentUniqueID(withVersion);
  // append pack common ID
  id += "[";
  id += GetPackageID(false);
  id += "]";
  return id;
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
  string prefix;
  if (!m_componentAttributes.IsApi())
    prefix = GetVendorAndBundle();
  string comment = m_componentAttributes.ConstructComponentID(prefix, false, false, false, ':');

  return comment;
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
    return t->GetFile(this, c);
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
  if (!IsConfig())
    return EMPTY_STRING; // nothing to do
  string src = GetAbsolutePath();
  return RteFsUtils::BackupFile(src, bDeleteExisting);
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
      string savedFile = RteUtils::AppendFileVersion(dst, GetVersionString(), true);
      project->MergeFiles(bak, dst, savedFile);
    }
  }

  return true;

}

bool RteFileInstance::Construct(XMLTreeElement* xmlElement)
{
  bool success = RteItemInstance::Construct(xmlElement);
  if (success) {
    // backward compatibility
    // replace copy="1" attribute with attr="config"
    const string& s = GetAttribute("copy");
    if (s == "1" || s == "true")
      SetAttribute("attr", "config");
    if (!s.empty())
      RemoveAttribute("copy");
    if (!IsConfig())
      return false; // will cause to delete the instance
  }

  if (m_componentAttributes.GetAttribute("Cvendor").empty()) {
    m_componentAttributes.AddAttribute("Cvendor", m_packageAttributes.GetAttribute("vendor"));
  }

  return success;
}


bool RteFileInstance::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  const string& tag = xmlElement->GetTag();
  if (tag == "instance") {
    const string& val = xmlElement->GetText();
    m_instanceName = val;
    m_fileName = RteUtils::ExtractFileName(m_instanceName);
    const string& sIndex = xmlElement->GetAttribute("index");
    if (!sIndex.empty())
      m_instanceIndex = stoi(sIndex);
    const string& removed = xmlElement->GetAttribute("removed");
    if (!removed.empty())
      m_bRemoved = stoi(removed) != 0;
    return true;
  } else if (tag == "component") {
    m_componentAttributes.SetAttributes(xmlElement->GetAttributes());
    return true;
  }
  return RteItemInstance::ProcessXmlElement(xmlElement);
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
  m_commonID = RteAttributes::GetPackageID(false);
  return RteAttributes::GetPackageID(true);
}


string RtePackageInstanceInfo::GetPackageID(bool withVersion) const
{
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
  for (auto it = m_targetInfos.begin(); it != m_targetInfos.end(); it++) {
    if (!ResolvePack(it->first))
      resolved = false;
  }
  return resolved;
}


bool RtePackageInstanceInfo::ResolvePack(const string& targetName)
{
  if (!IsUsedByTarget(targetName))
    return true;
  VersionCmp::MatchMode mode = GetVersionMatchMode(targetName);
  if (mode == VersionCmp::EXCLUDED_VERSION)
    return true;
  RtePackage* pack = NULL;
  RteModel* model = GetModel();
  if (mode == VersionCmp::FIXED_VERSION) {
    pack = model->GetPackage(GetPackageID(true));
  } else {
    pack = model->GetLatestPackage(GetPackageID(false));
  }

  if (pack)
    SetResolvedPack(pack, targetName);

  return pack != NULL;
}


RteGpdscInfo::RteGpdscInfo(RteItem* parent, RteGeneratorModel* model) :
  RteItemInstance(parent),
  m_model(model)
{
};

RteGpdscInfo::~RteGpdscInfo()
{
  if (m_model) {
    delete m_model;
    m_model = 0;
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
    return abs;
  }
  return name;// absolute
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
  m_packageAttributes.SetAttributes(package->GetAttributes());
}

void RteBoardInfo::InitInstance(RteItem* item) {
  if (!item)
    return;
  SetTag(item->GetTag());
  AddAttribute("Bname", item->GetName());
  AddAttribute("Bversion", item->GetAttribute("Bversion")); // for backward compatibility
  AddAttribute("Brevision", item->GetVersionString());
  AddAttribute("Bvendor", item->GetVendorString());
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
  for (auto it = m_targetInfos.begin(); it != m_targetInfos.end(); it++) {
    RteBoard* board = ResolveBoard(it->first);
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

  // get package info
  RtePackage* package = c->GetPackage();
  m_packageAttributes.SetAttributes(package->GetAttributes());
}

RteComponentInstance* RteComponentInstance::MakeCopy()
{
  if (m_copy)
    delete m_copy;
  m_copy = 0;
  m_copy = new RteComponentInstance(*this);
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

  for (auto it = m_targetInfos.begin(); it != m_targetInfos.end(); it++) {
    RteInstanceTargetInfo* tiThis = it->second;
    RteInstanceTargetInfo* tiThat = ci->GetTargetInfo(it->first);
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


bool RteComponentInstance::Construct(XMLTreeElement* xmlElement)
{
  bool success = RteItemInstance::Construct(xmlElement);
  if (success) {
  }
  return success;
}

string RteComponentInstance::GetComponentUniqueID(bool withVersion) const
{
  string id = RteAttributes::GetComponentUniqueID(withVersion);
  if (!IsApi()) {
    id += "[";
    id += GetPackageID(false);
    id += "]";
  }
  return id;
}

bool RteComponentInstance::HasAggregateID(const string& aggregateId) const
{
  string id = GetComponentAggregateID();
  if (id == aggregateId)
    return true;
  // check resolved components : they could have resolved components with bundles
  for (auto it = m_resolvedComponents.begin(); it != m_resolvedComponents.end(); it++) {
    RteComponent* c = it->second;
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
  m_ID = GetComponentUniqueID(true);
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
    for (auto it = m_targetInfos.begin(); it != m_targetInfos.end(); it++) {
      RteInstanceTargetInfo* ti = it->second;
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
  for (auto it = m_targetInfos.begin(); it != m_targetInfos.end(); it++) {
    RteInstanceTargetInfo* ti = it->second;
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
  for (auto it = m_targetInfos.begin(); it != m_targetInfos.end(); it++) {
    ResolveComponent(it->first);
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
  string prefix;
  if (!IsApi())
    prefix = GetVendorAndBundle();
  return ConstructComponentID(prefix, true, true, false, ':');
}


string RteComponentInstance::GetAggregateDisplayName() const
{
  string prefix;
  if (!IsApi())
    prefix = GetVendorAndBundle();
  return ConstructComponentID(prefix, false, false, false, ':');
}

string RteComponentInstance::GetDisplayName() const
{
  string prefix;
  if (!IsApi())
    prefix = GetVendorAndBundle();
  return ConstructComponentID(prefix, false, false, false, ':');
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
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(*it);
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
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(*it);
    if (ci && ci->IsModified())
      return true;
  }
  return false;
}

RteComponentInstance* RteComponentInstanceAggregate::GetModifiedInstance() const
{
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(*it);
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
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(*it);
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
    m_fullDisplayName = ci->GetAggregateDisplayName();
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
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    RteComponentInstance* ci = dynamic_cast<RteComponentInstance*>(*it);
    if (ci && ci->HasAggregateID(aggregateId))
      return true;
  }
  return false;
}

bool RteComponentInstanceAggregate::HasComponentInstance(RteComponentInstance* ci) const
{
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    if (ci == *it)
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
  map<string, RteComponentInstanceGroup*>::iterator it = m_groups.begin();
  for (; it != m_groups.end(); it++)
  {
    delete it->second;
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
  for (auto ita = m_children.begin(); ita != m_children.end(); ita++) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(*ita);
    if (!a) {
      continue; // should not happen
    }
    if (a->HasAggregateID(id)) {
      return a;
    }
  }
  map<string, RteComponentInstanceGroup*>::const_iterator it;
  for (it = m_groups.begin(); it != m_groups.end(); it++) {
    RteComponentInstanceGroup* g = it->second;
    RteComponentInstanceAggregate* a = g->GetComponentInstanceAggregate(id);
    if (a)
      return a;
  }
  return NULL;
}

RteComponentInstanceAggregate* RteComponentInstanceGroup::GetComponentInstanceAggregate(RteComponentInstance* ci) const
{
  for (auto ita = m_children.begin(); ita != m_children.end(); ita++) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(*ita);
    if (a && a->HasComponentInstance(ci)) {
      return a;
    }
  }
  map<string, RteComponentInstanceGroup*>::const_iterator it;
  for (it = m_groups.begin(); it != m_groups.end(); it++) {
    RteComponentInstanceGroup* g = it->second;
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

  for (auto ita = m_children.begin(); ita != m_children.end(); ita++) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(*ita);
    if (a && a->HasComponentInstance(ci)) {
      return const_cast<RteComponentInstanceGroup*>(this);
    }
  }
  map<string, RteComponentInstanceGroup*>::const_iterator it;
  for (it = m_groups.begin(); it != m_groups.end(); it++) {
    RteComponentInstanceGroup* g = it->second->GetComponentInstanceGroup(ci);
    if (g)
      return g;
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
  for (auto ita = m_children.begin(); ita != m_children.end(); ita++) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(*ita);
    aggregates.insert(a);
  }

  map<string, RteComponentInstanceGroup*>::const_iterator it;
  for (it = m_groups.begin(); it != m_groups.end(); it++) {
    RteComponentInstanceGroup* g = it->second;
    g->GetInstanceAggregates(aggregates);
  }
}


void RteComponentInstanceGroup::GetModifiedInstanceAggregates(set<RteComponentInstanceAggregate*>& modifiedAggregates) const
{
  for (auto ita = m_children.begin(); ita != m_children.end(); ita++) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(*ita);
    if (a && a->IsModified())
      modifiedAggregates.insert(a);
  }

  map<string, RteComponentInstanceGroup*>::const_iterator it;
  for (it = m_groups.begin(); it != m_groups.end(); it++) {
    RteComponentInstanceGroup* g = it->second;
    g->GetModifiedInstanceAggregates(modifiedAggregates);
  }
}


bool RteComponentInstanceGroup::HasUnresolvedComponents(const string& targetName, bool bCopy) const
{
  if (m_apiInstance && !m_apiInstance->GetResolvedComponent(targetName))
    return true;

  for (auto ita = m_children.begin(); ita != m_children.end(); ita++) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(*ita);
    if (a && a->IsUnresolved(targetName, bCopy))
      return true;
  }

  map<string, RteComponentInstanceGroup*>::const_iterator it;
  for (it = m_groups.begin(); it != m_groups.end(); it++) {
    RteComponentInstanceGroup* g = it->second;
    if (g->HasUnresolvedComponents(targetName, bCopy))
      return true;
  }
  return false;
}

bool RteComponentInstanceGroup::IsUsedByTarget(const string& targetName) const
{
  for (auto ita = m_children.begin(); ita != m_children.end(); ita++) {
    RteComponentInstanceAggregate* a = dynamic_cast<RteComponentInstanceAggregate*>(*ita);
    if (a && a->IsUsedByTarget(targetName))
      return true;
  }

  map<string, RteComponentInstanceGroup*>::const_iterator it;
  for (it = m_groups.begin(); it != m_groups.end(); it++) {
    RteComponentInstanceGroup* g = it->second;
    if (g->IsUsedByTarget(targetName))
      return true;
  }
  return false;
}

// End of RteInstance.cpp
