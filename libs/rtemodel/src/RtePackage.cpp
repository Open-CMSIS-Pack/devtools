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

#include "RteConstants.h"

#include "XMLTree.h"

#include <cstring>
using namespace std;


RteReleaseContainer::RteReleaseContainer(RteItem* parent) :
  RteItem(parent)
{
}

void RteReleaseContainer::Construct()
{
  string version;
  for (auto release : GetChildren()) {
    // First release XML node is the version of the package
    version = release->GetVersionString();
    break;
  }
  AddAttribute("version", version);
}

RtePackage::RtePackage(RteItem* parent, PackageState ps) :
  RteRootItem(parent),
  m_packState(ps),
  m_nDominating(-1),
  m_nDeprecated(-1),
  m_releases(0),
  m_licenseSets(0),
  m_conditions(0),
  m_components(0),
  m_apis(0),
  m_examples(0),
  m_taxonomy(0),
  m_boards(0),
  m_requirements(0),
  m_generators(0),
  m_groups(0),
  m_deviceFamilies(0)
{
}

RtePackage::RtePackage(RteItem* parent, const map<string, string>& attributes) :
  RteRootItem(parent),
  m_packState(PackageState::PS_UNKNOWN),
  m_nDominating(-1),
  m_nDeprecated(-1),
  m_releases(0),
  m_licenseSets(0),
  m_conditions(0),
  m_components(0),
  m_apis(0),
  m_examples(0),
  m_taxonomy(0),
  m_boards(0),
  m_requirements(0),
  m_generators(0),
  m_groups(0),
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
  m_licenseSets = 0;
  m_components = 0;
  m_requirements = 0,
  m_conditions = 0;
  m_apis = 0;
  m_examples = 0;
  m_taxonomy = 0;
  m_boards = 0;
  m_generators = 0;
  m_groups = 0;
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
  return RteUtils::GetPrefix(id, RteConstants::PREFIX_PACK_VERSION_CHAR);
}


string RtePackage::DisplayNameFromId(const string& id)
{
  return CommonIdFromId(id);
}

string RtePackage::VersionFromId(const string& id)
{
  return VersionCmp::RemoveVersionMeta(RteUtils::GetSuffix(id, RteConstants::PREFIX_PACK_VERSION_CHAR));
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
  return CommonIdFromId(id) + RteConstants::PREFIX_PACK_VERSION + version;
}


string RtePackage::VendorFromId(const string& id)
{
  return RteUtils::RemoveSuffixByString(id, RteConstants::SUFFIX_PACK_VENDOR);
}

string RtePackage::NameFromId(const string& id)
{
  string name;
  string::size_type pos = 0;
  string::size_type posEnd = 0;
  pos = id.find(RteConstants::SUFFIX_PACK_VENDOR);
  if (pos == string::npos) {
    return name;
  }
  pos+= strlen(RteConstants::SUFFIX_PACK_VENDOR);
  posEnd = id.find(RteConstants::PREFIX_PACK_VERSION_CHAR, pos);
  if (posEnd == string::npos) {
    return id.substr(pos);
  }
  return id.substr(pos, posEnd - pos);
}

std::string RtePackage::PackIdFromPath(const std::string& path)
{
  string baseName = RteUtils::ExtractFileBaseName(path);
  list<string> segments;
  RteUtils::SplitString(segments, baseName, '.');
  string commonID;
  string version;
  unsigned i = 0;
  for (string s : segments) {
    if (i == 0) {
      commonID = s;
    } else if (i == 1) {
      commonID += RteConstants::SUFFIX_PACK_VENDOR + s;
    } else if (i == 2) {
      version = RteConstants::PREFIX_PACK_VERSION + s;
    } else {
      version += '.' + s;
    }
    i++;
  }
  if (version.empty()) {
    // try upper directory
    string dir = RteUtils::ExtractFileName(RteUtils::ExtractFilePath(path, false));
    if (!dir.empty() && isdigit(dir.at(0))) {
      version = RteConstants::PREFIX_PACK_VERSION + dir;
    }
  }
  return commonID + version;
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

int RtePackage::ComparePdscFileNames(const std::string& pdsc1, const std::string& pdsc2)
{
  return ComparePackageIDs(PackIdFromPath(pdsc1), PackIdFromPath(pdsc2));
}

RtePackage* RtePackage::GetPackFromList(const std::string& packID, const std::list<RtePackage*>& packs)
{
  for(auto& pack : packs) {
    if(pack && pack->GetID() == packID) {
      return pack;
    }
  }
  return nullptr;
}

RteItem* RtePackage::GetRelease(const string& version) const
{
  if (m_releases == nullptr) {
    return nullptr;
  }

  for (auto pItem : m_releases->GetChildren()) {
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
  for (auto pItem : m_releases->GetChildren()) {
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
  for (auto pItem : m_releases->GetChildren()) {
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

void RtePackage::GetRequiredPacks(RtePackageMap& packs, RteModel* model) const
{
  RtePackage::ResolveRequiredPacks(this, GetPackRequirements(), packs, model);
}

void RtePackage::ResolveRequiredPacks(const RteItem* originatingItem,
        const Collection<RteItem*>& requirements, RtePackageMap& packs, RteModel* model)
{
  for (auto item : requirements) {
    string id = RtePackage::GetPackageIDfromAttributes(*item);
    if (packs.find(id) != packs.end()) {
      continue; // already in the map
    }
    RtePackage* pack = model ? model->GetPackage(*item) : nullptr;
    packs[id] = pack;
    if (pack && pack != originatingItem) {
      pack->GetRequiredPacks(packs, model);
    }
  }
}


const Collection<RteItem*>& RtePackage::GetPackRequirements() const
{
  return GetItemGrandChildren(m_requirements, "packages");
}

const Collection<RteItem*>& RtePackage::GetLanguageRequirements() const
{
  return GetItemGrandChildren(m_requirements, "languages");
}

const Collection<RteItem*>& RtePackage::GetCompilerRequirements() const
{
  return GetItemGrandChildren(m_requirements, "compilers");
}

string RtePackage::GetDownloadUrl(bool withVersion, const char* extension) const
{
  string packurl = GetAttribute("url");
  if (packurl.empty()) {
    return EMPTY_STRING;
  }
  if (packurl[packurl.size() - 1] != '/') {
    packurl += "/";
  }
  return packurl + GetPackageFileNameFromAttributes(*this, withVersion, extension);
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
    auto& gens = m_generators->GetChildren();
    auto it = gens.begin();
    if (it != gens.end())
      return dynamic_cast<RteGenerator*>(*it);
  }
  return nullptr;
}


RteComponent* RtePackage::GetComponent(const string& id) const
{
  if (m_components) {
    for (auto child : m_components->GetChildren()) {
      RteComponent* c = dynamic_cast<RteComponent*>(child);
      if (c && c->GetID() == id)
        return c;
    }
  }
  return nullptr;
}


RteItem* RtePackage::GetLicenseSet(const string& id) const
{
  if (m_licenseSets) {
    if (id.empty()) {
      return GetDefaultLicenseSet();
    }
    return m_licenseSets->GetItem(id);
  }
  return nullptr;
}

RteItem* RtePackage::GetDefaultLicenseSet() const
{
  if (m_licenseSets) {
    return m_licenseSets->GetDefaultChild();
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
  if (!m_deviceFamilies) {
    return;
  }
  for (auto child : m_deviceFamilies->GetChildren()) {
    RteDeviceFamily* fam = dynamic_cast<RteDeviceFamily*>(child);
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
  } else if (tag == "groups") {
      m_groups = new RteFileContainer(this);
    return m_groups;
  } else if (tag == "devices") {
      m_deviceFamilies = new RteDeviceFamilyContainer(this);
    return m_deviceFamilies;
  } else if (tag == "examples") {
      m_examples = new RteExampleContainer(this);
    return m_examples;
  } else if (tag == "conditions") {
      m_conditions = new RteConditionContainer(this);
    return m_conditions;
  } else if (tag == "licenseSets") {
    m_licenseSets = new RteItem(this);
    return m_licenseSets;
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
  EraseAttributes("xmlns:*");
  EraseAttributes("xs:*");
  // some XML readers strip namespaces
  RemoveAttribute("xs");
  RemoveAttribute("noNamespaceSchemaLocation");
  RemoveAttribute("schemaLocation");

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
    for (auto child : m_conditions->GetChildren()) {
      RteCondition* cond = dynamic_cast<RteCondition*>(child);
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
  m_nDominating = !m_nDeprecated && GetItemByTag("dominate") != nullptr;
  return id;
}


string RtePackage::GetPackageID(bool withVersion) const
{
  if (withVersion)
    return GetID();
  return GetCommonID();
}

string RtePackage::GetPackageIDfromAttributes(const XmlItem& attr, bool withVersion, bool useDots)
{
  const auto& vendor = attr.GetAttribute("vendor");
  const string version = withVersion ?
    VersionCmp::RemoveVersionMeta(attr.GetAttribute("version")) : EMPTY_STRING;

  return ComposePackageID(vendor, attr.GetAttribute("name"), version, useDots);
}

string RtePackage::ComposePackageID(const string& vendor, const string& name, const string& version, bool useDots) {
  const vector<pair<const char*, const string&>> elements = {
    {"",                  vendor},
    {vendor.empty() ? "" :
     useDots ? RteConstants::DOT_STR : RteConstants::SUFFIX_PACK_VENDOR,  name},
    {useDots ? RteConstants::DOT_STR : RteConstants::PREFIX_PACK_VERSION, version},
  };
  return RteUtils::ConstructID(elements);
};

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

std::string RtePackage::GetPackageFileNameFromAttributes(const XmlItem& attr, bool withVersion, const char* extension)
{
  string filename = GetPackageIDfromAttributes(attr, withVersion, true);
  if (extension) {
    filename += extension;
  }
  return filename;
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
  auto ps = pack ? pack->GetPackageState() : PackageState::PS_UNKNOWN;
  if(ps == PackageState::PS_INSTALLED || ps == PackageState::PS_GENERATED ||
     ps == PackageState::PS_EXPLICIT_PATH) {
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

RtePackageAggregate::RtePackageAggregate(const std::string& commonId) :
  RteItem(nullptr),
  m_mode(VersionCmp::MatchMode::LATEST_VERSION)
{
  m_ID = commonId;
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


RtePackage* RtePackageAggregate::GetPackage(const std::string& id) const
{
  RteItem* entry = GetPackageEntry(id);
  if(entry) {
    return entry->GetPackage();
  }
  return nullptr;
}

RteItem* RtePackageAggregate::GetPackageEntry(const string& id) const
{
  return get_or_null(m_packages, id);
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
  for (auto [_, p] : m_packages) {
    if(p) {
      RtePackage* pack = p->GetPackage();
      if(pack == p) { // only consider packs themselves
        return pack;
      }
    }
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

bool RtePackageAggregate::AddPackage(RtePackage* pack, bool overwrite)
{
  if(!pack) {
    return false;
  }
  if(overwrite || !GetPackage(pack->GetID())) {
    m_packages[pack->GetID()] = pack;
    if(m_ID.empty()) {
      m_ID = pack->GetCommonID();
    }
    return true;
  }
  return false;
}

void RtePackageAggregate::AddPackage(const string& packID, RteItem* pi)
{
  string version = RtePackage::VersionFromId(packID);
  if (!version.empty()) {
    auto it = m_packages.find(packID);
    if (it == m_packages.end())
      m_packages[packID] = pi;
  }
  if (m_ID.empty())
    m_ID = RtePackage::CommonIdFromId(packID);

}

const string& RtePackageAggregate::GetDescription() const
{
  RtePackage* p = GetLatestPackage();
  if(p) {
    return p->GetDescription();
  }

  static string NO_PACK("Pack is not installed");
  return NO_PACK;
}

const string& RtePackageAggregate::GetURL() const
{
  const string& url = GetAttribute("url");
  if (!url.empty())
    return url;

  for (auto [_, p] : m_packages) {
    if (p)
      return p->GetAttribute("url");
  }
  return EMPTY_STRING;
}
// -------------------------------------
RtePackFamilyCollection::RtePackFamilyCollection()
{
}

RtePackFamilyCollection::~RtePackFamilyCollection()
{

  m_aggregates.clear();
}


void RtePackFamilyCollection::Clear()
{
  for(auto [_, pa] : m_aggregates) {
    delete pa;
  }
  m_aggregates.clear();
}

RtePackage* RtePackFamilyCollection::GetPackage(const std::string& packID) const
{
  auto pa = GetPackageAggregate(packID);
  if(pa) {
    return pa->GetPackage(packID);
  }
  return nullptr;
}


 RteItem* RtePackFamilyCollection::GetLatestEntry(const std::string& packID) const
{
  auto pa = GetPackageAggregate(packID);
  if(pa) {
    return pa->GetLatestEntry();
  }
  return nullptr;
}

RtePackage* RtePackFamilyCollection::GetLatestPackage(const std::string& packID) const
{
  auto pa = GetPackageAggregate(packID);
  if(pa) {
    return pa->GetLatestPackage();
  }
  return nullptr;
}

const std::string& RtePackFamilyCollection::GetLatestPackageID(const std::string& commonId) const
{
  auto pa = GetPackageAggregate(commonId);
  if(pa) {
    return pa->GetLatestPackageID();
  }
  return RteUtils::EMPTY_STRING;
}

bool RtePackFamilyCollection::AddPackage(RtePackage* pack, bool overwrite )
{
  if(!pack) {
    return false;
  }
  RtePackageAggregate* pa = EnsurePackageAggregate(pack->GetCommonID());
  if(!pa->GetPackage(pack->GetID())) {
    pa->AddPackage(pack, overwrite);
  }
  return true;
}

void RtePackFamilyCollection::AddPackageEnry(const std::string& packID, RteItem* pi)
{
  RtePackageAggregate* pa = EnsurePackageAggregate(packID);
  pa->AddPackage(packID, pi);
}

RtePackageAggregate* RtePackFamilyCollection::GetPackageAggregate(const std::string& packID) const
{
  return get_or_null(m_aggregates, RtePackage::CommonIdFromId(packID));
}

RtePackageAggregate* RtePackFamilyCollection::EnsurePackageAggregate(const std::string& commonId)
{
  RtePackageAggregate* pa = GetPackageAggregate(commonId);
  if(!pa) {
    pa = new RtePackageAggregate(commonId);
    m_aggregates[commonId] = pa;
  }
  return pa;
}


// -------------------------------------
RtePackRegistry::RtePackRegistry()
{
}

RtePackRegistry::~RtePackRegistry()
{
  Clear();
}


void RtePackRegistry::Clear()
{
  for(auto [_, p] : m_loadedPacks) {
    delete p;
  }
  m_loadedPacks.clear();
}


RtePackage* RtePackRegistry::GetPack(const std::string& pdscFile) const
{
  return get_or_null(m_loadedPacks, pdscFile);
}

bool RtePackRegistry::AddPack(RtePackage* pack, bool bReplace)
{
  if(!pack) {
    return false;
  }
  auto& fileName = pack->GetPackageFileName();
  auto existing = GetPack(fileName);
  if(existing) {
    if(bReplace) {
      delete existing;
    } else {
      return false;
    }
  }
  m_loadedPacks[fileName] = pack;
  return true;
}

bool RtePackRegistry::ErasePack(const std::string& pdscFile)
{
  auto it = m_loadedPacks.find(pdscFile);
  if(it != m_loadedPacks.end()) {
    delete it->second;
    m_loadedPacks.erase(it);
    return true;
  }
  return false;
}


// End of RtePackage.cpp
