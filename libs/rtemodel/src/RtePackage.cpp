/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RtePackage.cpp
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RtePackage.h"

#include "RteCondition.h"
#include "RteComponent.h"
#include "RteInstance.h"
#include "RteModel.h"
#include "RteDevice.h"
#include "RteExample.h"
#include "RteGenerator.h"
#include "RteBoard.h"

#include "XMLTree.h"

using namespace std;


RteReleaseContainer::RteReleaseContainer(RteItem* parent) :
  RteItem(parent)
{
}

void RteReleaseContainer::Construct()
{
  string version;
  for (auto release : GetChildren()) {
    const string& releaseVersion = release->GetVersionString();
    if (version.empty() || VersionCmp::Compare(releaseVersion, version) > 0) {
      version = releaseVersion;
    }
  }
  AddAttribute("version", version);
}

RtePackage::RtePackage(RteItem* parent, PackageState ps) :
  RteRootItem(parent),
  m_packState(ps),
  m_nDominating(-1),
  m_nDeprecated(-1),
  m_releases(0),
  m_conditions(0),
  m_components(0),
  m_apis(0),
  m_examples(0),
  m_taxonomy(0),
  m_boards(0),
  m_requirements(0),
  m_generators(0),
  m_deviceFamilies(0)
{
}

RtePackage::RtePackage(RteItem* parent, const map<string, string>& attributes) :
  RteRootItem(parent),
  m_packState(PackageState::PS_UNKNOWN),
  m_nDominating(-1),
  m_nDeprecated(-1),
  m_releases(0),
  m_conditions(0),
  m_components(0),
  m_apis(0),
  m_examples(0),
  m_taxonomy(0),
  m_boards(0),
  m_requirements(0),
  m_generators(0),
  m_deviceFamilies(0)
{
  SetAttributes(attributes);
  m_ID = RtePackage::ConstructID();
}


RtePackage::~RtePackage()
{
  RtePackage::Clear();
}

void RtePackage::Clear()
{
  m_nDeprecated = -1;
  m_releases = 0;
  m_components = 0;
  m_requirements = 0,
  m_conditions = 0;
  m_apis = 0;
  m_examples = 0;
  m_taxonomy = 0;
  m_boards = 0;
  m_generators = 0;
  m_deviceFamilies = 0;
  m_nDominating = -1;
  m_keywords.clear();
  RteItem::Clear();
}

bool RtePackage::IsDeprecated() const
{
  if (m_nDeprecated >= 0)
    return m_nDeprecated > 0;

  RteItem* latestRelease = GetLatestRelease();
  if (latestRelease) {
    return !latestRelease->GetItemValue("deprecated").empty(); // deprecated attribute contains date
  }
  return false;
}

bool RtePackage::IsDominating() const
{
  if (m_nDominating >= 0)
    return m_nDominating > 0;

  if (IsDeprecated())
    return false;

  RteItem* dominate = GetItemByTag("dominate");
  return dominate != nullptr;
}

string RtePackage::GetDisplayName() const
{
  string id = GetVendorString();
  id += "::";
  id += GetName();
  return id;
}

string RtePackage::CommonIdFromId(const string& id)
{
  string::size_type pos = 0;
  pos = id.find('.');
  if (pos == string::npos)
    return id;
  pos++;
  pos = id.find('.', pos);
  if (pos == string::npos)
    return id;
  return id.substr(0, pos);
}


string RtePackage::DisplayNameFromId(const string& id)
{
  string displayName;
  string::size_type vendorPos = 0;
  string::size_type namePos = 0;
  vendorPos = id.find('.');
  if (vendorPos == string::npos)
    return id;
  displayName = id.substr(0, vendorPos);
  vendorPos++;
  namePos = id.find('.', vendorPos);
  if (namePos == string::npos)
    return id;
  displayName += "::";
  displayName += id.substr(vendorPos, namePos - vendorPos);
  return displayName;

}

string RtePackage::VersionFromId(const string& id)
{
  string version;
  string::size_type pos = 0;
  pos = id.find('.');
  if (pos == string::npos)
    return version;
  pos++;
  pos = id.find('.', pos);
  if (pos == string::npos)
    return version;
  pos++;
  version = id.substr(pos);
  return VersionCmp::RemoveVersionMeta(version);
}

string RtePackage::ReleaseVersionFromId(const string& id)
{
  string version = VersionFromId(id);
  if (version.empty())
    return version;

  // drop the suffix: starts with first dash or a non-numeric character in the last segment
  // try dash
  string::size_type pos = 0;
  pos = version.find('-');
  if (pos != string::npos)
    return version.substr(0, pos);

  // otherwise get first non-numeric character in the last available segment
  string::size_type lastPos = 0;
  pos = 0;

  for (int i = 0; i < 2; i++) {
    pos = version.find('.', lastPos);
    if (pos == string::npos)
      break;
    lastPos = pos + 1;
  }

  if (lastPos == 0)
    return version;

  for (pos = lastPos + 1; pos < version.length(); pos++) {
    char ch = version.at(pos);
    if (!isdigit(ch))
      return version.substr(0, pos);
  }
  return version;
}

string RtePackage::ReleaseIdFromId(const string& id)
{
  string version = ReleaseVersionFromId(id);
  if (version.empty())
    return id;
  string releaseId = CommonIdFromId(id);
  releaseId += ".";
  releaseId += version;
  return releaseId;
}


string RtePackage::VendorFromId(const string& id)
{
  string vendor;

  string::size_type pos = 0;
  pos = id.find('.');
  if (pos == string::npos)
    return vendor;
  vendor = id.substr(0, pos);
  return vendor;

}

string RtePackage::NameFromId(const string& id)
{
  string name;
  string::size_type pos = 0;
  string::size_type posEnd = 0;
  pos = id.find('.');
  if (pos == string::npos) {
    return name;
  }
  pos++;
  posEnd = id.find('.', pos);
  if (posEnd == string::npos) {
    return id.substr(pos);
  }
  return id.substr(pos, posEnd - pos);
}

int RtePackage::ComparePackageIDs(const string& a, const string& b)
{
  string commonA = RtePackage::CommonIdFromId(a);
  string commonB = RtePackage::CommonIdFromId(b);

  // place Keil DFP's always at the end: packs from device vendors should have priority
  bool bKeilA = commonA.find("Keil") == 0;
  bool bKeilB = commonB.find("Keil") == 0;
  if (bKeilA != bKeilB) {
    if (bKeilA)
      return 1;
    else if (bKeilB)
      return -1;
  }
  int res = AlnumCmp::CompareLen(commonA, commonB);

  if (res != 0) {
    return res;
  }

  string verA = RtePackage::VersionFromId(a);
  string verB = RtePackage::VersionFromId(b);
  return VersionCmp::Compare(verB, verA); // reverse comparison!
}

RteItem* RtePackage::GetRelease(const string& version) const
{
  if (m_releases == nullptr) {
    return nullptr;
  }

  const list<RteItem*>& children = m_releases->GetChildren();
  for (auto it = children.begin(); it != children.end(); it++) {
    RteItem *pItem = *it;
    if (pItem == nullptr) {
      continue;
    }

    if (VersionCmp::Compare(pItem->GetVersionString(), version) == 0) {
      return pItem;
    }
  }

  return nullptr;
}


const string& RtePackage::GetReleaseText(const string& version) const
{
  RteItem* item = GetRelease(version);
  if (item) {
    return item->GetText();
  }
  return RteUtils::EMPTY_STRING;
}


RteItem* RtePackage::GetLatestRelease() const
{
  if (m_releases == nullptr) {
    return nullptr;
  }

  const list<RteItem*>& children = m_releases->GetChildren();
  for (auto it = children.begin(); it != children.end(); it++) {
    RteItem *pItem = *it;
    if (pItem == nullptr) {
      continue;
    }

    const string &relVersion = pItem->GetVersionString();
    const string &packVersion = GetVersionString();
    if (VersionCmp::Compare(relVersion, packVersion) == 0) { // case sensitive
      return pItem;
    }
  }
  return nullptr;
}


const string& RtePackage::GetLatestReleaseText() const
{
  RteItem* latestRelease = GetLatestRelease();
  if (latestRelease) {
    return latestRelease->GetText();
  }
  return EMPTY_STRING;
}


bool RtePackage::ReleaseVersionExists(const string& version) {
  if (m_releases == nullptr) {
    return false;
  }
  const list<RteItem*>& children = m_releases->GetChildren();
  for (auto it = children.begin(); it != children.end(); it++) {
    RteItem *pItem = *it;
    if (pItem == nullptr) {
      continue;
    }
    if (VersionCmp::Compare(version, pItem->GetVersionString()) == 0) {
      return true;
    }
  }
  return false;
}


const string& RtePackage::GetReplacement() const {
  RteItem* latestRelease = GetLatestRelease();
  if (latestRelease) {
    return latestRelease->GetAttribute("replacement");
  }
  return EMPTY_STRING;
}


const string& RtePackage::GetReleaseDate() const {
  RteItem* latestRelease = GetLatestRelease();
  if (latestRelease) {
    return latestRelease->GetAttribute("date");
  }
  return EMPTY_STRING;
}


const string& RtePackage::GetReleaseDate(const string& version) const {
  RteItem* release = GetRelease(version);
  if (release) {
    return release->GetAttribute("date");
  }
  return EMPTY_STRING;
}


string RtePackage::GetDatedVersion() const {
  const string& version = GetVersionString();
  return GetDatedVersion(version);
}


string RtePackage::GetDatedVersion(const string& version) const {
  const string& date = GetReleaseDate(version);
  if (date.empty()) {
    return version;
  }
  return (version + " (" + date + ")");
}


const string& RtePackage::GetDeprecationDate() const {
  RteItem* latestRelease = GetLatestRelease();
  if (latestRelease) {
    return latestRelease->GetAttribute("deprecated");
  }
  return EMPTY_STRING;
}

XMLTreeElement* RtePackage::CreatePackXmlTreeElement(XMLTreeElement* parent) const
{
  XMLTreeElement* package = new XMLTreeElement(parent, "package");
  if(parent) {
    parent->AddChild(package);
  }
  package->CreateElement("name", GetName());
  package->CreateElement("vendor", GetVendorString());
  package->CreateElement("url", GetURL());
  package->CreateElement("description", GetDescription());
  XMLTreeElement* releases = package->CreateElement("releases");
  RteItem* latestRelease = GetLatestRelease();
  if(latestRelease) {
    releases->CreateElement("release")->SetAttributes(latestRelease->GetAttributes());
  } else {
    // should actually never come here
    releases->CreateElement("release")->AddAttribute("version", "0.0.0");
  }
  return package;
}

const list<RteItem*>& RtePackage::GetPackRequirements() const
{
  return GetItemGrandChildren(m_requirements, "packages");
}

const list<RteItem*>& RtePackage::GetLanguageRequirements() const
{
  return GetItemGrandChildren(m_requirements, "languages");
}

const list<RteItem*>& RtePackage::GetCompilerRequirements() const
{
  return GetItemGrandChildren(m_requirements, "compilers");
}

string RtePackage::GetDownloadUrl(bool withVersion, const char* extension) const
{
  string url = EMPTY_STRING;
  string packurl = GetAttribute("url");
  if (packurl.empty()) {
    return EMPTY_STRING;
  }
  if (packurl[packurl.size() - 1] != '/') {
    packurl += "/";
  }
  url = packurl;
  if (withVersion) {
    url += GetID();
  } else {
    url += GetCommonID();
  }
  if (extension != nullptr) {
    url += extension;
  }
  return url;
}

RteComponent* RtePackage::FindComponents(const RteItem& item, list<RteComponent*>& components) const
{
  if (item.IsApi()) {
    return m_apis ? m_apis->FindComponents(item, components) : nullptr;
  }
  return m_components ? m_components->FindComponents(item, components) : nullptr;
}

RteApi* RtePackage::GetApi(const map<string, string>& componentAttributes) const
{
  if (m_apis) {
    map<string, RteApi*>::const_iterator it;
    for (auto item : m_apis->GetChildren()) {
      RteApi* a = dynamic_cast<RteApi*>(item);
      if (a && a->MatchApiAttributes(componentAttributes))
        return a;
    }
  }
  return nullptr;
}

RteApi* RtePackage::GetApi(const string& id) const
{
  if (m_apis) {
    map<string, RteApi*>::const_iterator it;
    for (auto item : m_apis->GetChildren()) {
      RteApi* a = dynamic_cast<RteApi*>(item);
      if (a && a->GetID() == id)
        return a;
    }
  }
  return nullptr;
}


RteGenerator* RtePackage::GetGenerator(const string& id) const
{
  if (m_generators)
    return m_generators->GetGenerator(id);
  return nullptr;
}

RteGenerator* RtePackage::GetFirstGenerator() const
{
  if (m_generators) {
    const list<RteItem*>& gens = m_generators->GetChildren();
    auto it = gens.begin();
    if (it != gens.end())
      return dynamic_cast<RteGenerator*>(*it);
  }
  return nullptr;
}


RteComponent* RtePackage::GetComponent(const string& id) const
{
  if (m_components) {
    const list<RteItem*>& comps = m_components->GetChildren();
    for (auto it = comps.begin(); it != comps.end(); it++) {
      RteComponent* c = dynamic_cast<RteComponent*>(*it);
      if (c && c->GetID() == id)
        return c;
    }
  }
  return nullptr;
}


RteCondition* RtePackage::GetCondition(const string& id) const
{
  if (m_conditions)
    return dynamic_cast<RteCondition*>(m_conditions->GetItem(id));
  return nullptr;
}

const RteItem* RtePackage::GetTaxonomyItem(const std::string& id) const
{
  if (m_taxonomy) {
    for (auto t : m_taxonomy->GetChildren()) {
      if (t->GetTaxonomyDescriptionID() == id) {
        return t;
      }
    }
  }
  return nullptr;
}


const std::string& RtePackage::GetTaxonomyDescription(const std::string& id) const
{
  const RteItem* t = GetTaxonomyItem(id);
  if (t) {
    return t->GetText();
  }
  return EMPTY_STRING;
}

const std::string RtePackage::GetTaxonomyDoc(const std::string& id) const
{
  const RteItem* t = GetTaxonomyItem(id);
  if (t) {
    return t->GetDocFile();
  }
  return EMPTY_STRING;
}

void RtePackage::GetEffectiveDeviceItems(list<RteDeviceItem*>& devices) const
{
  if (!m_deviceFamilies || m_deviceFamilies->GetChildCount() == 0)
    return;

  const list<RteItem*>& children = m_deviceFamilies->GetChildren();
  for (auto itfam = children.begin(); itfam != children.end(); itfam++) {
    RteDeviceFamily* fam = dynamic_cast<RteDeviceFamily*>(*itfam);
    if (fam) {
      fam->GetEffectiveDeviceItems(devices);
    }
  }
}

RteItem* RtePackage::CreateItem(const std::string& tag)
{
  if (tag == "components") {
    m_components = new RteComponentContainer(this);
    return m_components;
  } else if (tag == "apis") {
      m_apis = new RteApiContainer(this);
      return m_apis;
  } else if (tag == "generators") {
      m_generators = new RteGeneratorContainer(this);
    return m_generators;
  } else if (tag == "devices") {
      m_deviceFamilies = new RteDeviceFamilyContainer(this);
    return m_deviceFamilies;
  } else if (tag == "examples") {
      m_examples = new RteExampleContainer(this);
    return m_examples;
  } else if (tag == "conditions") {
      m_conditions = new RteConditionContainer(this);
    return m_conditions;
  } else if (tag == "releases") {
      m_releases = new RteReleaseContainer(this);
      return m_releases;
  } else if (tag == "requirements") {
      m_requirements = new RteItem(this);
      return m_requirements;
  } else if (tag == "taxonomy") {
      m_taxonomy = new RteItem(this);
      return m_taxonomy;
  } else if (tag == "boards") {
      m_boards = new RteBoardContainer(this);
    return m_boards;
  }
  return RteRootItem::CreateItem(tag);
}


void RtePackage::Construct()
{
  // remove attributes that we do not need:
  RemoveAttribute("xmlns:xs");
  RemoveAttribute("xs:noNamespaceSchemaLocation");
  // some XML readers strip namespaces
  RemoveAttribute("xs");
  RemoveAttribute("noNamespaceSchemaLocation");

  if (m_releases) {
    AddAttribute("version", m_releases->GetVersionString());
  }
  for (auto keyword : GetGrandChildren("keywords")) {
    m_keywords.insert(keyword->GetText());
  }
  RteRootItem::Construct();
}


bool RtePackage::Validate()
{
  if (!RteItem::Validate())
    m_bValid = false;
  if (m_conditions) {
    map<string, bool> conditionIDs;
    const list<RteItem*>& conds = m_conditions->GetChildren();
    for (auto it = conds.begin(); it != conds.end(); it++) {
      RteCondition* cond = dynamic_cast<RteCondition*>(*it);
      if (!cond)
        continue;

      const string& id = cond->GetID();
      map<string, bool>::iterator itcond = conditionIDs.find(id);
      if (itcond != conditionIDs.end()) {
        if (itcond->second)
          continue; // already reported
        string msg = GetPackageFileName();
        msg += ": warning #521: condition '" + id + "' is defined more than once - duplicate is ignored";
        m_errors.push_front(msg);
        conditionIDs[id] = true;
        m_bValid = false;
      } else {
        conditionIDs[id] = false;
      }
    }
  }
  return m_bValid;
}

void RtePackage::InsertInModel(RteModel* model)
{
  if (m_components)
    m_components->InsertInModel(model);
  if (m_apis)
    m_apis->InsertInModel(model);
}

string RtePackage::ConstructID()
{
  if (!HasAttribute("name")) {
    AddAttribute("name", GetChildText("name"));
  }

  if (!HasAttribute("vendor")) {
    AddAttribute("vendor", GetChildText("vendor"));
  }

  if (!HasAttribute("url")) {
    AddAttribute("url", GetChildText("url"));
  }

  string id = RtePackage::GetPackageIDfromAttributes(*this, true);
  m_commonID = RtePackage::GetPackageIDfromAttributes(*this, false);

  m_nDeprecated = IsDeprecated() ? 1 : 0;
  m_nDominating = IsDominating() ? 1 : 0;
  return id;
}


string RtePackage::GetPackageID(bool withVersion) const
{
  if (withVersion)
    return GetID();
  return GetCommonID();
}


string RtePackage::GetPackageIDfromAttributes(const XmlItem& attr, bool withVersion)
{
  string id = attr.GetAttribute("vendor");
  if (!id.empty())
    id += ".";
  id += attr.GetAttribute("name");

  if (withVersion) {
    const string& version = attr.GetAttribute("version");
    if (!version.empty()) {
      id += ".";
      id += VersionCmp::RemoveVersionMeta(version);
    }
  }
  return id;
}


string RtePackage::GetPackagePath(bool withVersion) const
{
  string path = GetVendorString();
  if (!path.empty())
    path += "/";
  path += GetName();
  path += "/";
  if (withVersion && !GetVersionString().empty()) {
    path += VersionCmp::RemoveVersionMeta(GetVersionString());
    path += "/";
  }
  return path;
}


RtePackageInfo::RtePackageInfo(RtePackage* pack)
  : RteItem(pack)
{
  Init(pack, RteUtils::EMPTY_STRING);
}

RtePackageInfo::RtePackageInfo(RtePackage* pack, const string& version)
  : RteItem(pack)
{
  Init(pack, version);
}



string RtePackageInfo::ConstructID()
{
  m_commonID = RtePackage::GetPackageIDfromAttributes(*this, false);
  return RtePackage::GetPackageIDfromAttributes(*this, true);
}


const string& RtePackageInfo::GetCommonID() const
{
  return m_commonID;
}


string RtePackageInfo::GetDisplayName() const
{
  RtePackage* pack = GetPackage();
  if (pack == nullptr) {
    return EMPTY_STRING;
  }
  return pack->GetDisplayName();
}

string RtePackageInfo::GetPackageID(bool withVersion) const
{
  RtePackage* pack = GetPackage();
  if (pack) {
    return pack->GetPackageID(withVersion);
  }
  if (withVersion)
    return GetID();
  return GetCommonID();
}


string RtePackageInfo::GetPackagePath(bool withVersion) const
{
  string path = RteItem::GetPackagePath(false);
  if (path.empty()) {
    return path;
  }
  if (withVersion && !GetVersionString().empty()) {
    path += VersionCmp::RemoveVersionMeta(GetVersionString()) + "/";
  }
  return path;
}


string RtePackageInfo::GetDownloadUrl(bool withVersion, const char* extension) const
{
  string url = RteItem::GetDownloadUrl(false, nullptr);
  if (url.empty()) {
    return EMPTY_STRING;
  }
  if (withVersion && !GetVersionString().empty()) {
    url += ".";
    url += GetVersionString();
  }
  if (extension != nullptr) {
    url += extension;
  }
  return url;
}

string RtePackageInfo::GetDownloadReleaseUrl(bool withVersion, const char* extension, bool useReleaseUrl) const {
  string url;
  if (useReleaseUrl) {
    // release url has priority. Ignore extension and version.
    std::string vers = GetVersionString();
    RtePackage* package = GetPackage();
    if (package) {
      RteItem *release = package->GetRelease(vers);
      if (release) {
        url = release->GetAttribute("url");
        if (!url.empty()) {
          return url;
        }
      }
    }
  }
  return GetDownloadUrl(withVersion, extension);
}

bool RtePackageInfo::IsLatestRelease() const
{
  RtePackage* pack = GetPackage();
  if (pack == nullptr) {
    return false;
  }
  return (GetVersionString() == pack->GetVersionString());
}


RteDeviceFamilyContainer* RtePackageInfo::GetDeviceFamiles() const
{
  RtePackage* pack = GetPackage();
  if (pack == nullptr) {
    return nullptr;
  }
  return pack->GetDeviceFamiles();
}


string RtePackageInfo::GetAbsolutePackagePath() const
{
  RtePackage* pack = GetPackage();
  if (pack && pack->GetPackageState() == PackageState::PS_INSTALLED) {
    return pack->GetAbsolutePackagePath();
  }
  // No sense in returning this path if not installed
  return RteUtils::EMPTY_STRING;
}


const string& RtePackageInfo::GetReleaseText() const
{
  return GetReleaseText(GetVersionString());
}


const string& RtePackageInfo::GetReleaseText(const string& version) const
{
  RtePackage* pack = GetPackage();
  if (pack) {
    return pack->GetReleaseText(version);
  }
  return EMPTY_STRING;
}


const string& RtePackageInfo::GetLatestReleaseText() const
{
  RtePackage* pack = GetPackage();
  if (pack) {
    return pack->GetLatestReleaseText();
  }
  return EMPTY_STRING;
}


bool RtePackageInfo::ReleaseVersionExists(const string& version)
{
  RtePackage* pack = GetPackage();
  if (pack) {
    return pack->ReleaseVersionExists(version);
  }
  return false;
}


const string& RtePackageInfo::GetReplacement() const
{
  RtePackage* pack = GetPackage();
  if (pack) {
    return pack->GetReplacement();
  }
  return EMPTY_STRING;
}


const string& RtePackageInfo::GetReleaseDate() const
{
  RtePackage* pack = GetPackage();
  if (pack) {
    return pack->GetReleaseDate();
  }
  return EMPTY_STRING;
}


const string& RtePackageInfo::GetReleaseDate(const string& version) const
{
  RtePackage* pack = GetPackage();
  if (pack) {
    return pack->GetReleaseDate(version);
  }
  return EMPTY_STRING;
}


string RtePackageInfo::GetDatedVersion() const
{
  RtePackage* pack = GetPackage();
  if (pack) {
    return pack->GetDatedVersion(GetVersionString());
  }
  return EMPTY_STRING;
}


string RtePackageInfo::GetDatedVersion(const string& version) const
{
  RtePackage* pack = GetPackage();
  if (pack) {
    return pack->GetDatedVersion(version);
  }
  return EMPTY_STRING;
}


const string& RtePackageInfo::GetDeprecationDate() const
{
  RtePackage* pack = GetPackage();
  if (pack) {
    return pack->GetDeprecationDate();
  }
  return EMPTY_STRING;
}


RteItem* RtePackageInfo::GetExamples() const
{
  RtePackage* pack = GetPackage();
  if (pack) {
    return pack->GetExamples();
  }
  return nullptr;
}


RteItem* RtePackageInfo::GetBoards() const
{
  RtePackage* pack = GetPackage();
  if (pack) {
    return pack->GetBoards();
  }
  return nullptr;
}

const string& RtePackageInfo::GetRepository() const {
  RtePackage* pack = GetPackage();
  if (pack) {
    RteItem *repository = pack->GetItemByTag("repository");
    if (repository) {
      return repository->GetText();
    }
  }
  return EMPTY_STRING;
}

const string& RtePackageInfo::GetReleaseAttributeValue(const string& attribute, const string &version, bool latest/* = true*/) const {
  RtePackage* pack = GetPackage();
  if (pack) {
    RteItem *release = latest ? pack->GetLatestRelease() : pack->GetRelease(version);
    if (release) {
      return release->GetAttribute(attribute);
    }
  }
  return EMPTY_STRING;
}

void RtePackageInfo::Init(RtePackage* pack, const string& version) {
  SetAttributes(pack->GetAttributes());
  AddAttribute("description", pack->GetDescription());
  if (!version.empty()) {
    AddAttribute("version", version);
  }
  m_ID = ConstructID();
  m_bValid = true;
  SetText(pack->GetText());
}


RtePackageFilter::~RtePackageFilter()
{
  Clear();
}

void RtePackageFilter::Clear()
{
  m_selectedPacks.clear();
  m_latestPacks.clear();
  m_latestInstalledPacks.clear();
}

bool RtePackageFilter::IsEqual(const RtePackageFilter& other) const
{

  if (m_bUseAllPacks != other.IsUseAllPacks())
    return false;

  if (m_selectedPacks != other.GetSelectedPackages())
    return false;

  if (m_latestPacks != other.GetLatestPacks())
    return false;

  return true;
}


bool RtePackageFilter::SetSelectedPackages(const std::set<std::string>& packs)
{
  bool equal = m_selectedPacks.size() == packs.size();
  if (equal) {
    for (auto& it : packs) {
      if (m_selectedPacks.find(it) == m_selectedPacks.end()) {
        equal = false;
        break;
      }
    }
  }
  if (!equal) {
    m_selectedPacks = packs;
    return true;
  }
  return false;
}

bool RtePackageFilter::SetLatestPacks(const std::set<std::string>& latestPacks)
{
  bool equal = m_latestPacks == latestPacks;
  if (equal) {
    return false;
  }
  m_latestPacks = latestPacks;
  return true;
}

bool RtePackageFilter::AreAllExcluded() const
{
  return !m_bUseAllPacks && m_selectedPacks.empty() && m_latestPacks.empty();
}

bool RtePackageFilter::IsUseAllPacks() const
{
  return m_bUseAllPacks && m_selectedPacks.empty() && m_latestPacks.empty();
}

bool RtePackageFilter::IsPackageSelected(const string& packId) const
{
  return m_selectedPacks.find(packId) != m_selectedPacks.end();
}

bool RtePackageFilter::IsPackageSelected(RtePackage* pack) const
{
  return pack && IsPackageSelected(pack->GetID());
}

bool RtePackageFilter::IsPackageExcluded(const string& packId) const
{
  return !IsPackageFiltered(packId);
}


bool RtePackageFilter::IsPackageExcluded(RtePackage* pack) const
{
  return !pack || !IsPackageFiltered(pack->GetID());
}

bool RtePackageFilter::IsPackageFiltered(RtePackage* pack) const
{
  return pack && IsPackageFiltered(pack->GetID());
}

bool RtePackageFilter::IsPackageFiltered(const string& packId) const
{
  if (!IsUseAllPacks()) {
    if (IsPackageSelected(packId))
      return true;

    string commonId = RtePackage::CommonIdFromId(packId);
    if (m_latestPacks.find(commonId) == m_latestPacks.end())
      return false;

    for (auto& it : m_selectedPacks) {
      string id = RtePackage::CommonIdFromId(it);
      if (id == commonId) {
        return false; // another pack version is explicitly selected
      }
    }
  }
  if (m_latestInstalledPacks.find(packId) != m_latestInstalledPacks.end())
    return true;
  return false;
}



RtePackageAggregate::RtePackageAggregate(RteItem* parent) :
  RteItem(parent),
  m_mode(VersionCmp::MatchMode::EXCLUDED_VERSION)
{
}

RtePackageAggregate::~RtePackageAggregate()
{
  RtePackageAggregate::Clear();
}

void RtePackageAggregate::Clear()
{
  m_packages.clear();
  m_selectedPackages.clear();
  m_usedPackages.clear();

  RteItem::Clear();
}

RteItem* RtePackageAggregate::GetPackage(const string& id) const
{
  auto it = m_packages.find(id);
  if (it != m_packages.end()) {
    return it->second;
  }
  return nullptr;
}

const string& RtePackageAggregate::GetLatestPackageID() const
{
  auto it = m_packages.begin();
  if (it != m_packages.end()) {
    return it->first;
  }
  return EMPTY_STRING;
}

RteItem* RtePackageAggregate::GetLatestEntry() const
{
  auto it = m_packages.begin();
  if (it != m_packages.end())
    return (it->second);
  return nullptr;
}


RtePackage* RtePackageAggregate::GetLatestPackage() const
{
  for (auto it = m_packages.begin(); it != m_packages.end(); it++) {
    RtePackage* p = dynamic_cast<RtePackage*>(it->second);
    if (p)
      return p;
  }
  return nullptr;
}

bool RtePackageAggregate::IsPackageUsed(const string& id) const
{
  return m_usedPackages.find(id) != m_usedPackages.end();
}


bool RtePackageAggregate::IsPackageSelected(const string& id) const
{
  return m_selectedPackages.find(id) != m_selectedPackages.end();
}

void RtePackageAggregate::SetPackageUsed(const string& id, bool bUsed)
{
  if (bUsed) {
    m_usedPackages.insert(id);
  } else {
    auto it = m_usedPackages.find(id);
    if (it != m_usedPackages.end())
      m_usedPackages.erase(it);
  }
}
void RtePackageAggregate::SetPackageSelected(const string& id, bool bSelected)
{
  string version = RtePackage::VersionFromId(id);
  if (version.empty())
    return;
  if (bSelected) {
    m_selectedPackages.insert(id);
    m_mode = VersionCmp::MatchMode::FIXED_VERSION;
  } else {
    auto it = m_selectedPackages.find(id);
    if (it != m_selectedPackages.end())
      m_selectedPackages.erase(it);
  }
}

void RtePackageAggregate::SetVersionMatchMode(VersionCmp::MatchMode mode)
{
  m_mode = mode;
  if (m_mode == VersionCmp::MatchMode::FIXED_VERSION) {
    string version = RtePackage::VersionFromId(GetLatestPackageID());
    if (version.empty()) {
      m_mode = VersionCmp::MatchMode::LATEST_VERSION; // missing latest pack, cannot set to FIXED version
    } else if (m_selectedPackages.empty()) {
      SetPackageSelected(GetLatestPackageID(), true);
    }
  }
}

void RtePackageAggregate::AdjustVersionMatchMode()
{
  if (IsUsed()) {
    string version = RtePackage::VersionFromId(GetLatestPackageID());
    if (version.empty())
      SetVersionMatchMode(VersionCmp::MatchMode::LATEST_VERSION);
    else
      SetVersionMatchMode(VersionCmp::MatchMode::FIXED_VERSION); // ensure selection not empty
  }
}

void RtePackageAggregate::AddPackage(RtePackage* pack)
{
  if (pack) {
    m_packages[pack->GetID()] = pack;
    if (m_displayName.empty())
      m_displayName = pack->GetDisplayName();
  }
}

void RtePackageAggregate::AddPackage(const string& packID, RteItem* pi)
{
  string version = RtePackage::VersionFromId(packID);
  if (!version.empty()) {
    auto it = m_packages.find(packID);
    if (it == m_packages.end())
      m_packages[packID] = pi;
  }
  if (m_displayName.empty())
    m_displayName = RtePackage::DisplayNameFromId(packID);

}

const string& RtePackageAggregate::GetDescription() const
{
  RtePackage* p = GetLatestPackage();
  if (p)
    return p->GetDescription();

  static string NO_PACK("Pack is not installed");
  return NO_PACK;
}

const string& RtePackageAggregate::GetURL() const
{
  const string& url = GetAttribute("url");
  if (!url.empty())
    return url;

  for (auto it = m_packages.begin(); it != m_packages.end(); it++) {
    RteItem* p = it->second;
    if (p)
      return p->GetAttribute("url");
  }
  return EMPTY_STRING;
}

// End of RtePackage.cpp
