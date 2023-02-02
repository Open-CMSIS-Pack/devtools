/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteFile.cpp
* @brief CMSIS RTE Data model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteFile.h"

#include "RteComponent.h"
#include "RteModel.h"

#include "XMLTree.h"

using namespace std;

RteFile::RteFile(RteItem* parent) :
  RteItem(parent)
{
}

bool RteFile::Construct(XMLTreeElement* xmlElement)
{
  if (!RteItem::Construct(xmlElement))
    return false;
  // backward compatibility
  // replace copy="1" attribute with attr="config"
  const string& s = GetAttribute("copy");
  if (s == "1" || s == "true")
    SetAttribute("attr", "config");
  if (!s.empty())
    RemoveAttribute("copy");

  return true;
}

bool RteFile::Validate()
{
  m_bValid = true; // assume valid
  const string& conditionId = GetConditionID();
  if (!conditionId.empty()) {
    if (!GetCondition(conditionId)) {
      string msg = "file '";
      msg += GetName();
      msg += "': condition '";
      msg += conditionId;
      msg += "' not found";
      m_errors.push_back(msg);
      m_bValid = false;
    }
  }
  return m_bValid;
}


RteFile::Role RteFile::GetRole() const
{
  return RoleFromString(GetAttribute("attr"));
}

bool RteFile::IsForcedCopy() const
{
  return GetRole() == ROLE_COPY;
}

bool RteFile::IsConfig() const
{
  return GetRole() == ROLE_CONFIG;
}

bool RteFile::IsTemplate() const
{
  return GetRole() == ROLE_TEMPLATE;
}

bool RteFile::IsAddToProject() const
{
  if (IsConfig()) {
    return true;
  }
  Category cat = GetCategory();
  if (cat == SOURCE || cat == SOURCE_ASM || cat == SOURCE_C || cat == SOURCE_CPP ||
    cat == LIBRARY || cat == OBJECT) {
    return !IsTemplate(); // templates are not added to project as is
  }
  return false;
}


RteFile::Category RteFile::GetCategory() const
{
  return CategoryFromString(GetCategoryString());
}

const string& RteFile::GetCategoryString() const
{
  return GetAttribute("category");
}

string RteFile::GetFileComment() const
{
  string comment;
  RteComponent* c = GetComponent();
  if (c) {
    comment = "(";
    comment += c->ConstructComponentDisplayName(false, false);
    comment += ")";
  }
  return comment;
}

string RteFile::GetHeaderComment() const
{
  string comment;
  RteComponent* c = GetComponent();
  if (c)
    comment = c->GetFullDisplayName();
  return comment;
}

string RteFile::ConstructID()
{
  string id = GetName();
  SetAttribute("name", id.c_str());

  if (!GetVersionString().empty()) {
    id += ".";
    id += GetVersionString();
  }
  return id;
}

string RteFile::GetIncludePath() const
{
  if (HasAttribute("path")) {
    return GetOriginalAbsolutePath(GetAttribute("path"));
  }
  return RteUtils::ExtractFilePath(GetOriginalAbsolutePath(), false);
}


string RteFile::GetIncludeFileName() const
{
  if (HasAttribute("path")) {
    string path = GetOriginalAbsolutePath(GetAttribute("path")) + "/";
    string fileName = GetOriginalAbsolutePath();
    if (!path.empty() && fileName.find(path) == 0) {
      return fileName.substr(path.length());
    }
  }
  return RteUtils::ExtractFileName(GetName());
}


void RteFile::GetAbsoluteSourcePaths(set<string>& paths) const
{
  const string& src = GetAttribute("src");
  if (src.empty())
    return;
  string packagePath;
  RtePackage* p = GetPackage();
  if (p) {
    packagePath = p->GetAbsolutePackagePath();
  }
  string::size_type posFrom, posTo, count;
  // split the source paths sources can be separated by semicolon
  for (posFrom = 0; posFrom < src.length();) {
    posTo = src.find(';', posFrom);

    if (posTo != string::npos) {
      count = posTo - posFrom;
      posTo++;
    } else {
      count = string::npos;
    }
    string s = src.substr(posFrom, count);
    if (!s.empty())
      paths.insert(packagePath + s);
    if (count == string::npos)
      break;
    posFrom = posTo;
  }
}


string RteFile::GetInstancePathName(const string& deviceName, int instanceIndex, const string& rteFolder) const
{
  string pathName;
  RteComponent* c = GetComponent();
  if (c) {
    bool bConfig = IsConfig();
    bool bTemplate = IsTemplate();
    bool bForcedCopy = IsForcedCopy();
    if (bConfig || bTemplate || bForcedCopy) {
      if (bConfig || bForcedCopy) {
        // replace all ' ' with '_' in class name, the generated path should not contain spaces
        pathName += rteFolder + "/" + RteUtils::SpacesToUnderscore(c->GetCclassName()) + "/";
        if (!bForcedCopy && !deviceName.empty() && c->IsDeviceDependent()) {
          string device = WildCards::ToX(deviceName);
          if (!device.empty()) {
            pathName += device;
            pathName += "/";
          }
        }
      }
      string fullName = GetIncludeFileName(); // valid for all file categories
      string fileName = RteUtils::ExtractFileName(fullName);
      // add file path in case of relative header
      string filePath = RteUtils::ExtractFilePath(fullName, true);
      if (!filePath.empty()) {
        pathName += filePath;
      }
      // split name to add multi-instance items
      string baseName = RteUtils::ExtractFileBaseName(fileName);
      string ext = RteUtils::ExtractFileExtension(fileName);
      pathName += baseName;
      int maxInstances = c->GetMaxInstances();
      if (!bForcedCopy && c->HasMaxInstances() && instanceIndex >= 0 && instanceIndex < maxInstances) {
        pathName += "_";
        pathName += std::to_string(instanceIndex);
      }
      pathName += ".";
      pathName += ext;

    } else {
      pathName = GetOriginalAbsolutePath();
    }
  }
  return pathName;
}

RteFile::Role RteFile::RoleFromString(const string& role)
{
  if (role == "copy") {
    return ROLE_COPY;
  } else if (role == "config") {
    return ROLE_CONFIG;
  } else if (role == "template") {
    return ROLE_TEMPLATE;
  } else if (role == "interface") {
    return ROLE_INTERFACE;
  }
  return ROLE_NONE;
}

RteFile::Category RteFile::CategoryFromString(const string& category)
{
  if (category == "doc")
    return DOC;
  else if (category == "header")
    return HEADER;
  else if (category == "include")
    return INCLUDE;
  else if (category == "library")
    return LIBRARY;
  else if (category == "object")
    return OBJECT;
  else if (category == "source")
    return SOURCE;
  else if (category == "sourceAsm")
    return SOURCE_ASM;
  else if (category == "sourceC")
    return SOURCE_C;
  else if (category == "sourceCpp")
    return SOURCE_CPP;
  else if (category == "linkerScript")
    return LINKER_SCRIPT;
  else if (category == "utility")
    return UTILITY;
  else if (category == "svd") // deprecated
    return SVD;
  else if (category == "image")
    return IMAGE;
  else if (category == "preIncludeGlobal")
    return PRE_INCLUDE_GLOBAL;
  else if (category == "preIncludeLocal")
    return PRE_INCLUDE_LOCAL;
  return OTHER;
}

const string& RteFile::GetVersionString() const
{
  const string& ver = GetAttribute("version");
  if (!ver.empty())
    return ver;
  RteComponent* c = GetComponent();
  if (c)
    return c->GetVersionString();
  return EMPTY_STRING;
}


///////////////////////////////////////////////////////
RteFileContainer::RteFileContainer(RteItem* parent) :
  RteItem(parent)
{

}

RteFile* RteFileContainer::GetFile(const string& name) const
{
  for (auto itf = m_children.begin(); itf != m_children.end(); itf++) {
    RteFile* f = dynamic_cast<RteFile*>(*itf);
    if (f && f->GetName() == name)
      return f;
  }
  return nullptr;
}

RteFile* RteFileContainer::GetFileByOriginalAbsolutePath(const string& absPathName) const
{
  for (auto itf = m_children.begin(); itf != m_children.end(); itf++) {
    RteFile* f = dynamic_cast<RteFile*>(*itf);
    if (f && f->GetOriginalAbsolutePath() == absPathName)
      return f;
  }
  return nullptr;
}

RteFileContainer* RteFileContainer::GetParentContainer() const
{
  return dynamic_cast<RteFileContainer*>(GetParent());
}

string RteFileContainer::GetHierarchicalGroupName() const
{
  string groupName;
  RteFileContainer* parentGroup = GetParentContainer();
  const string& name = GetAttribute("name");
  if (parentGroup) {
    groupName = parentGroup->GetHierarchicalGroupName();
    if (!groupName.empty())
      groupName += ":";
  }
  groupName += name;
  return groupName;
}

void RteFileContainer::GetIncludePaths(set<string>& incPaths) const {
  const list<RteItem*>& children = GetChildren();
  for (auto itf = children.begin(); itf != children.end(); itf++) {
    RteItem* child = *itf;
    RteFile* f = dynamic_cast<RteFile*>(child);
    if (f) {
      RteFile::Category cat = f->GetCategory();
      if (cat == RteFile::INCLUDE) {
        string path = RteUtils::RemoveTrailingBackslash(f->GetOriginalAbsolutePath());
        if (!path.empty())
          incPaths.insert(path);
      } else if (cat == RteFile::HEADER) {
        string path = RteUtils::ExtractFilePath(f->GetOriginalAbsolutePath(), false);
        if (!path.empty())
          incPaths.insert(path);
      }
      continue;
    }
    RteFileContainer* g = dynamic_cast<RteFileContainer*>(child);
    if (g) {
      g->GetIncludePaths(incPaths);
    }
  }
}

void RteFileContainer::GetLinkerScripts(set<RteFile*>& linkerScripts) const {
  const list<RteItem*>& children = GetChildren();
  for (auto itf = children.begin(); itf != children.end(); itf++) {
    RteItem* child = *itf;
    RteFile* f = dynamic_cast<RteFile*>(child);
    if (f) {
      RteFile::Category cat = f->GetCategory();
      if (cat == RteFile::LINKER_SCRIPT) {
        linkerScripts.insert(f);
      }
      continue;
    }
    RteFileContainer* g = dynamic_cast<RteFileContainer*>(child);
    if (g) {
      g->GetLinkerScripts(linkerScripts);
    }
  }
}


bool RteFileContainer::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  const string& tag = xmlElement->GetTag();
  if (tag == "file") {
    RteFile* file = new RteFile(this);
    if (file->Construct(xmlElement)) {
      AddItem(file);
      return true;
    }
    delete file;
    return false;
  }
  if (tag == "group") {
    RteFileContainer* group = new RteFileContainer(this);
    if (group->Construct(xmlElement)) {
      AddItem(group);
      return true;
    }
    delete group;
    return false;
  }
  return RteItem::ProcessXmlElement(xmlElement);
}


RteFileTemplate::RteFileTemplate(const string& select) :
  m_select(select),
  m_instanceCount(0)
{
}

void RteFileTemplate::AddFile(RteFile* f)
{
  if (f) {
    m_files.insert(f);
  }
}

RteFileTemplateCollection::RteFileTemplateCollection(RteComponent* c) :
  m_component(c)
{
}

RteFileTemplateCollection::~RteFileTemplateCollection()
{
  for (auto it = m_templates.begin(); it != m_templates.end(); it++) {
    delete it->second;
  }
  m_templates.clear();
}

RteFileTemplate* RteFileTemplateCollection::GetTemplate(const string& select) const
{
  auto it = m_templates.find(select);
  if (it != m_templates.end())
    return it->second;
  return nullptr;
}

void RteFileTemplateCollection::AddFile(RteFile* f, int instanceCount)
{
  if (!f || !f->IsTemplate())
    return;
  const string& select = f->GetAttribute("select");
  if (select.empty())
    return;
  RteFileTemplate* t = GetTemplate(select);
  if (!t) {
    t = new RteFileTemplate(select);
    m_templates[select] = t;
    t->SetInstanceCount(instanceCount);
  }
  t->AddFile(f);
}

// End of RteFile.cpp
