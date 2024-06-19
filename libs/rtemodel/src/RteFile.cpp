/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteFile.cpp
* @brief CMSIS RTE Data model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
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

bool RteFile::IsForcedCopy() const
{
  return GetRole() == Role::ROLE_COPY;
}

bool RteFile::IsConfig() const
{
  return GetRole() == Role::ROLE_CONFIG;
}

bool RteFile::IsTemplate() const
{
  return GetRole() == Role::ROLE_TEMPLATE;
}

bool RteFile::IsAddToProject() const
{
  if (IsConfig()) {
    return true;
  }
  switch (GetCategory()) {
  case Category::SOURCE:
  case Category::SOURCE_ASM:
  case Category::SOURCE_C:
  case Category::SOURCE_CPP:
  case Category::LIBRARY:
  case Category::OBJECT:
    return !IsTemplate(); // templates are not added to project as is
  default:
    break;
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

RteFile::Role RteFile::GetRole() const
{
  return RoleFromString(GetAttribute("attr"));
}

RteFile::Scope RteFile::GetScope() const
{
  return ScopeFromString(GetAttribute("scope"));
}

RteFile::Language RteFile::GetLanguage() const
{
  return LanguageFromString(GetAttribute("language"));
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
  if (c) {
    comment = c->GetPartialComponentID(false);
  }
  return comment;
}

string RteFile::ConstructID()
{
  string id = GetName();
  if (!GetVersionString().empty()) {
    id += ".";
    id += GetVersionString();
  }
  return id;
}

const std::string& RteFile::GetName() const
{
  if(HasAttribute("file")) {
    return GetAttribute("file");
  }
  return RteItem::GetName();
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

RteFile::Category RteFile::CategoryFromString(const string& category)
{
  static const map<string, RteFile::Category> stringToCat = {
    {"doc", Category::DOC},
    {"header", Category::HEADER },
    {"include", Category::INCLUDE},
    {"library", Category::LIBRARY},
    {"object", Category::OBJECT},
    {"source", Category::SOURCE},
    {"sourceAsm", Category::SOURCE_ASM},
    {"sourceC", Category::SOURCE_C},
    {"sourceCpp", Category::SOURCE_CPP},
    {"linkerScript", Category::LINKER_SCRIPT},
    {"utility", Category::UTILITY},
    {"svd", Category::SVD}, // deprecated
    {"image", Category::IMAGE},
    {"preIncludeGlobal", Category::PRE_INCLUDE_GLOBAL},
    {"preIncludeLocal", Category::PRE_INCLUDE_LOCAL},
    {"genSource", Category::GEN_SOURCE},
    {"genHeader", Category::GEN_HEADER},
    {"genParams", Category::GEN_PARAMS},
    {"genAsset", Category::GEN_ASSET}
  };

  auto it = stringToCat.find(category);
  if (it != stringToCat.end()) {
    return it->second;
  }
  return Category::OTHER;
}

RteFile::Role RteFile::RoleFromString(const string& role)
{
  if (role == "copy") {
    return Role::ROLE_COPY;
  } else if (role == "config") {
    return Role::ROLE_CONFIG;
  } else if (role == "template") {
    return Role::ROLE_TEMPLATE;
  } else if (role == "interface") {
    return Role::ROLE_INTERFACE;
  }
  return Role::ROLE_NONE;
}

RteFile::Scope  RteFile::ScopeFromString(const std::string& scope)
{
  if (scope == "visible") {
    return Scope::SCOPE_PUBLIC;
  } else if (scope == "private") {
    return Scope::SCOPE_PRIVATE;
  }
  return Scope::SCOPE_NONE;
}

RteFile::Language RteFile::LanguageFromString(const std::string& language)
{
  if (language == "asm") {
    return Language::LANGUAGE_ASM;
  } else if (language == "c") {
    return Language::LANGUAGE_C;
  } else if (language == "cpp") {
    return Language::LANGUAGE_CPP;
  } else if (language == "c-cpp") {
    return Language::LANGUAGE_C_CPP;
  } else if (language == "link") {
    return Language::LANGUAGE_LINK;
  }
  return Language::LANGUAGE_NONE;
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
  for (auto itf : m_children) {
    RteFile* f = dynamic_cast<RteFile*>(itf);
    if (f && f->GetName() == name)
      return f;
  }
  return nullptr;
}

RteFile* RteFileContainer::GetFileByOriginalAbsolutePath(const string& absPathName) const
{
  for (auto itf : m_children) {
    RteFile* f = dynamic_cast<RteFile*>(itf);
    if (f && f->GetOriginalAbsolutePath() == absPathName)
      return f;
  }
  return nullptr;
}

RteFileContainer* RteFileContainer::GetParentContainer() const
{
  return dynamic_cast<RteFileContainer*>(GetParent());
}

const std::string& RteFileContainer::GetName() const
{
  if(HasAttribute("group")) {
    return GetAttribute("group");
  }
  return GetAttribute("name");
}


string RteFileContainer::GetHierarchicalGroupName() const
{
  const string& name = GetName();
  RteFileContainer* parentGroup = GetParentContainer();
  if (parentGroup) {
    string groupName = parentGroup->GetHierarchicalGroupName();
    if(!name.empty() && !groupName.empty()) {
      groupName += ":" + name;
    }
    return groupName;
  }
  return name;
}

void RteFileContainer::GetIncludePaths(set<string>& incPaths) const {
  for (auto child : GetChildren()) {
    RteFile* f = dynamic_cast<RteFile*>(child);
    if (f) {
      RteFile::Category cat = f->GetCategory();
      if (cat == RteFile::Category::INCLUDE) {
        string path = RteUtils::RemoveTrailingBackslash(f->GetOriginalAbsolutePath());
        if (!path.empty())
          incPaths.insert(path);
      } else if (cat == RteFile::Category::HEADER) {
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
  for (auto child : GetChildren()) {
    RteFile* f = dynamic_cast<RteFile*>(child);
    if (f) {
      RteFile::Category cat = f->GetCategory();
      if (cat == RteFile::Category::LINKER_SCRIPT) {
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


RteItem* RteFileContainer::CreateItem(const std::string& tag)
{
  if (tag == "file") {
    return new RteFile(this);
  } else if (tag == "group" || tag == "groups" || tag == "files") {
    return new RteFileContainer(this);
  } else if(tag == "-") {
    if(GetTag() == "files") {
      return new RteFile(this);
    } else {
      return new RteFileContainer(this);
    }
  }
  return RteItem::CreateItem(tag);
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
  for (auto it = m_templates.begin(); it != m_templates.end(); ++it) {
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
