/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteItem.cpp
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteItem.h"

#include "RteCondition.h"
#include "RtePackage.h"
#include "RteProject.h"
#include "RteModel.h"
#include "RteGenerator.h"
#include "RteConstants.h"

#include "RteFsUtils.h"
#include "XMLTree.h"
#include "CrossPlatformUtils.h"

using namespace std;

RteItem RteItem::EMPTY_RTE_ITEM(nullptr);


const std::string& RteItem::ConditionResultToString(RteItem::ConditionResult res)
{
  static vector<string> CONDITION_RESULT_STRINGS{
      "UNDEFINED",            // not evaluated yet
      "R_ERROR",              // error evaluating condition ( recursion detected, condition is missing)
      "FAILED",               // HW or compiler not match
      "MISSING",              // no component is installed
      "MISSING_API",          // no required api is installed
      "MISSING_API_VERSION",  // no api of required version is installed
      "UNAVAILABLE",          // component is installed, but filtered out
      "UNAVAILABLE_PACK",     // component is installed, pack is not selected
      "INCOMPATIBLE",         // incompatible component is selected
      "INCOMPATIBLE_VERSION", // incompatible version of component is selected
      "INCOMPATIBLE_VARIANT", // incompatible variant of component is selected
      "CONFLICT",             // several exclusive or incompatible components are selected
      "INSTALLED",            // matching component is installed, but not selectable because not in active bundle
      "SELECTABLE",           // matching component is installed, but not selected
      "FULFILLED",            // required component selected or no dependency exists
      "IGNORED"               // condition/expression is irrelevant for the current context
  };
  if (res < R_ERROR || res > IGNORED) {
    res = UNDEFINED;
  }
  return CONDITION_RESULT_STRINGS[res];
}


RteItem::RteItem(RteItem* parent) :
  XmlTreeItem<RteItem>(parent),
  m_bValid(false)
{
}

RteItem::RteItem(const std::string& tag, RteItem* parent):
  XmlTreeItem<RteItem>(parent, tag),
  m_bValid(false)
{
}

RteItem::RteItem(const std::map<std::string, std::string>& attributes, RteItem* parent) :
  XmlTreeItem<RteItem>(parent, attributes),
  m_bValid(true)
{
};

RteItem::~RteItem()
{
  RteItem::Clear(); // must be so: explicit call to this class implementation!
}


void RteItem::Clear()
{
  m_bValid = false;
  m_errors.clear();
  XmlTreeItem::Clear();
}


RteCallback* RteItem::GetCallback() const
{
  if (m_parent) {
    RteCallback* callback = m_parent->GetCallback();
    if(callback) {
      return callback;
    }
  }
  return RteCallback::GetGlobal(); // return global one: never nullptr
}


RteModel* RteItem::GetModel() const
{
  if (m_parent) {
    return m_parent->GetModel();
  }
  return nullptr;
}


RtePackage* RteItem::GetPackage() const
{
  if (m_parent)
    return m_parent->GetPackage();
  return nullptr;
}

bool RteItem::IsDeprecated() const
{
  const string& val = GetItemValue("deprecated");
  if (val == "1" || val == "true")
    return true;

  RtePackage* pack = GetPackage();
  if (pack && pack != this)
    return pack->IsDeprecated();
  return false;
}


RteProject* RteItem::GetProject() const
{
  for (RteItem* pItem = GetParent(); pItem != nullptr; pItem = pItem->GetParent()) {
    RteProject* pInstance = dynamic_cast<RteProject*>(pItem);
    if (pInstance)
      return pInstance;
  }
  return nullptr;
}

RteComponent* RteItem::GetComponent() const
{
  if (m_parent)
    return m_parent->GetComponent();
  return nullptr;
}

const string& RteItem::GetID() const
{
  return m_ID;
}

string RteItem::GetDependencyExpressionID() const
{
  return GetTag() + " " + GetComponentID(true);
}

string RteItem::GetComponentID(bool withVersion) const // to use in filtered list
{
  if (IsApi()) {
    return GetApiID(withVersion);
  }
  return ConstructComponentID(withVersion);
}

string RteItem::GetComponentUniqueID() const
{
  if (IsApi()) {
    return GetApiID(true);
  }
  string id = ConstructComponentID(true);
  id += RteConstants::OBRACE_STR;
  id += GetConditionID();
  id += RteConstants::CBRACE_STR;
  id += RteConstants::OSQBRACE_STR;
  id += GetPackageID(false);
  id += RteConstants::CSQBRACE_STR;
  return id;
}


string RteItem::GetComponentAggregateID() const
{
  const auto& vendor = GetVendorString();
  const vector<pair<const char*, const string&>> elements = {
    {"",              vendor},
    {vendor.empty() ? "" :
     RteConstants::SUFFIX_CVENDOR,  GetCclassName()},
    {RteConstants::PREFIX_CBUNDLE,  GetCbundleName()},
    {RteConstants::PREFIX_CGROUP,   GetCgroupName()},
    {RteConstants::PREFIX_CSUB,     GetCsubName()},
  };
  return RteUtils::ConstructID(elements);
}

string RteItem::GetPartialComponentID(bool bWithBundle) const {
  const vector<pair<const char*, const string&>> elements = {
    {"",                            GetCclassName()},
    {RteConstants::PREFIX_CBUNDLE,  bWithBundle ? GetCbundleName() : RteUtils::EMPTY_STRING},
    {RteConstants::PREFIX_CGROUP,   GetCgroupName()},
    {RteConstants::PREFIX_CSUB,     GetCsubName()},
    {RteConstants::PREFIX_CVARIANT, GetCvariantName()},
  };
  return RteUtils::ConstructID(elements);
}


string RteItem::GetApiID(bool withVersion) const
{
  string id = RteConstants::SUFFIX_CVENDOR;
  id += GetCclassName();
  id += RteConstants::PREFIX_CGROUP;
  id += GetCgroupName();

  id += "(API)";
  if (withVersion) {
    const string& ver = GetAttribute("Capiversion");
    if (!ver.empty()) {
      id += RteConstants::PREFIX_CVERSION;
      id += ver;
    }
  }
  return id;
}

string RteItem::ConcatenateCclassCgroupCsub(char delimiter) const
{
  string str = GetCclassName();
  str += delimiter;
  str += GetCgroupName();
  // optional subgroup
  if (!GetCsubName().empty()) {
    str += delimiter;
    str += GetCsubName();
  }
  return str;
}


void RteItem::SetAttributesFomComponentId(const std::string& componentId)
{
  ClearAttributes();
  if (componentId.empty()) {
    return;
  }
  string id = componentId;
  if (componentId.find(RteConstants::SUFFIX_CVENDOR) != string::npos) {
    string vendor = RteUtils::RemoveSuffixByString(id, RteConstants::SUFFIX_CVENDOR);
    AddAttribute("Cvendor", vendor);
    id = RteUtils::RemovePrefixByString(componentId, RteConstants::SUFFIX_CVENDOR);
  }
  AddAttribute("Cversion", RteUtils::GetSuffix(id, RteConstants::PREFIX_CVERSION_CHAR));
  id = RteUtils::GetPrefix(id, RteConstants::PREFIX_CVERSION_CHAR);
  list<string> segments;
  RteUtils::SplitString(segments, id, RteConstants::COLON_CHAR);
  size_t index = 0;
  for (auto s : segments) {
    switch (index) {
    case 0: // Cclass[&Cbundle]
      AddAttribute("Cclass", RteUtils::GetPrefix(s, RteConstants::PREFIX_CBUNDLE_CHAR));
      AddAttribute("Cbundle", RteUtils::GetSuffix(s, RteConstants::PREFIX_CBUNDLE_CHAR), false);
      break;
    case 1: // Cgroup[&Cvariant]
      AddAttribute("Cgroup", RteUtils::GetPrefix(s, RteConstants::PREFIX_CVARIANT_CHAR));
      AddAttribute("Cvariant", RteUtils::GetSuffix(s, RteConstants::PREFIX_CVARIANT_CHAR), false);
      break;
    case 2: // Csub
      AddAttribute("Csub", RteUtils::GetPrefix(s, RteConstants::PREFIX_CVARIANT_CHAR));
      AddAttribute("Cvariant", RteUtils::GetSuffix(s, RteConstants::PREFIX_CVARIANT_CHAR), false);
      break;
    default:
      break;
    };
    index++;
  }
}


string RteItem::ConstructComponentID(bool bVersion) const
{
  const auto& vendor = GetVendorString();
  const auto& version = bVersion ? GetVersionString() : EMPTY_STRING;
  const vector<pair<const char*, const string&>> elements = {
    {"",                            vendor},
    {vendor.empty() ? "" :
     RteConstants::SUFFIX_CVENDOR,  GetCclassName()},
    {RteConstants::PREFIX_CBUNDLE,  GetCbundleName()},
    {RteConstants::PREFIX_CGROUP,   GetCgroupName()},
    {RteConstants::PREFIX_CSUB,     GetCsubName()},
    {RteConstants::PREFIX_CVARIANT, GetCvariantName()},
    {RteConstants::PREFIX_CVERSION, version},
  };
  return RteUtils::ConstructID(elements);
}

string RteItem::ConstructComponentDisplayName(bool bClass, bool bVariant, bool bVersion, char delimiter) const
{
  string id;
  if (bClass) {
    id += GetCclassName();
    id += delimiter;
  }
  id += GetCgroupName();
  // optional subgroup
  if (!GetCsubName().empty()) {
    id += delimiter;
    id += GetCsubName();
  }

  // optional variant
  if (bVariant && !GetCvariantName().empty()) {
    id += RteConstants::PREFIX_CVARIANT;
    id += GetCvariantName();
  }

  if (bVersion) {
    const string& ver = GetVersionString();
    if (!ver.empty()) {
      id += RteConstants::PREFIX_CVERSION;
      id += ver;
    }
  }
  return id;
}


const Collection<RteItem*>& RteItem::GetItemChildren(RteItem* item)
{
  if (item && item->GetChildCount() > 0)
    return item->GetChildren();

  static const Collection<RteItem*> EMPTY_ITEM_LIST;
  return EMPTY_ITEM_LIST;
}

const Collection<RteItem*>& RteItem::GetItemGrandChildren(RteItem* item, const string& tag)
{
  RteItem* child = 0;
  if (item) {
    child = item->GetItemByTag(tag);
  }
  return GetItemChildren(child);
}

RteItem* RteItem::GetChildByTagAndAttribute(const string& tag, const string& attribute, const string& value) const
{
  for (auto child : GetChildren()) {
    if ((child->GetTag() == tag) && (child->GetAttribute(attribute) == value))
      return child;
  }
  return nullptr;
}

Collection<RteItem*>& RteItem::GetChildrenByTag(const std::string& tag, Collection<RteItem*>& items) const
{
  for (auto child : GetChildren()) {
    if ((child->GetTag() == tag)) {
      items.push_back(child);
    }
  }
  return items;
}


RteItem* RteItem::GetItem(const string& id) const
{
  for (auto item : m_children) {
    if (item->GetID() == id)
      return item;
  }
  return nullptr;
}

bool RteItem::HasItem(RteItem* item) const
{
  for (auto child : m_children) {
    if (item == child)
      return true;
  }
  return false;
}

RteItem* RteItem::GetItemByTag(const string& tag) const
{
  return GetFirstChild(tag);
}


const string& RteItem::GetDocValue() const
{
  const string& doc = GetItemValue("doc");
  if (!doc.empty())
    return doc;
  return GetDocAttribute();
}

const string& RteItem::GetDocAttribute() const
{
  const string& doc = GetAttribute("doc");
  if (!doc.empty())
    return doc;
  return GetAttribute("name");
}

const string& RteItem::GetRteFolder() const
{
  const string& rtePath = GetAttribute("rtedir");
  return rtePath;
}

const string& RteItem::GetVendorString() const
{
  if (IsApi())
    return EMPTY_STRING;
  const string& cv = GetAttribute("Cvendor");
  if (!cv.empty())
    return cv;
  return GetItemValue("vendor");
}

string RteItem::GetVendorName() const
{
  const string& vendor = GetVendorString();
  return DeviceVendor::GetCanonicalVendorName(vendor);
};

const string& RteItem::GetVersionString() const
{
  if (IsApi())
    return GetApiVersionString();
  const string& ver = GetAttribute("Cversion");
  if (!ver.empty())
    return ver;
  return GetAttribute("version");
}


void RteItem::RemoveItem(RteItem* item)
{
  RemoveChild(item, false);
}

string RteItem::ConstructID()
{
  string id = GetAttribute("id");
  if (!id.empty()) {
    return id;
  }
  id = GetName();
  if (!GetVersionString().empty()) {
    id += ".";
    id += GetVersionString();
  }
  return id;
}

string RteItem::CreateErrorString(const char* severity, const char* errNum, const char* message) const
{
  string msg = GetPackageID() + ": " + GetTag() + " '" + GetID() + "': " + severity + " #" + errNum + ": " + message;
  return msg;
}

string RteItem::GetDisplayName() const
{
  return GetID();
}


string RteItem::GetFullDeviceName() const
{
  string fullDeviceName;
  const string& variant = GetDeviceVariantName();
  if (!variant.empty())
    fullDeviceName = variant;
  else
    fullDeviceName = GetDeviceName();
  const string& processor = GetProcessorName();
  if (!processor.empty()) {
    fullDeviceName += ":";
    fullDeviceName += processor;
  }
  return fullDeviceName;
}

const std::string& RteItem::GetYamlDeviceAttribute(const string& rteName, const string& defaultValue)
{
  const string& rteValue = GetAttribute(rteName);
  if(rteValue.empty()) {
    return defaultValue;
  }
  const string& yamlValue = RteConstants::GetDeviceAttribute(rteName, rteValue);
  return !yamlValue.empty() ? yamlValue : rteValue;
}

string RteItem::GetProjectGroupName() const
{
  return string(RteConstants::SUFFIX_CVENDOR) + GetCclassName();
}

string RteItem::GetBundleShortID() const
{
  return GetBundleID(false);
};

string RteItem::GetBundleID(bool bWithVersion) const
{
  if (!GetCbundleName().empty()) {
    string s = GetVendorString();
    if (!s.empty()) {
      s += RteConstants::SUFFIX_CVENDOR;
    }
    s += GetCclassName() + RteConstants::PREFIX_CBUNDLE + GetCbundleName();
    if (bWithVersion && !GetVersionString().empty()) {
      s += RteConstants::PREFIX_CVERSION;
      s += GetVersionString();
    }
    return s;
  }
  return EMPTY_STRING;
}

string RteItem::GetTaxonomyDescriptionID() const
{
  return RteItem::GetTaxonomyDescriptionID(*this);
}

string RteItem::GetTaxonomyDescriptionID(const XmlItem& attributes)
{
  string taxonomyId =  attributes.GetAttribute("Cclass"); // taxonomy must have at least Cclass attribute
  if (!taxonomyId.empty()) {
    const string& group = attributes.GetAttribute("Cgroup");
    if (!group.empty()) {
      taxonomyId += RteConstants::PREFIX_CGROUP + group;
      const string& sub = attributes.GetAttribute("Csub");
      if (!sub.empty()) {
        taxonomyId += RteConstants::PREFIX_CSUB + sub;
      }
    }
  }
  return taxonomyId;
}

string RteItem::GetPackageID(bool withVersion) const
{
  RtePackage* package = GetPackage();
  if (!package || package == this) {
    return RtePackage::GetPackageIDfromAttributes(*this, withVersion);
  }
  return package->GetPackageID(withVersion);
}

string RteItem::GetPackagePath(bool withVersion) const
{
  string path = EMPTY_STRING;
  RtePackage* package = GetPackage();
  if (package) {
    path = package->GetPackagePath(withVersion);
  }
  return path;
}

PackageState RteItem::GetPackageState() const
{
  RtePackage* package = GetPackage();
  if (package) {
    return package->GetPackageState();
  }
  return PackageState::PS_UNKNOWN;

}

const string& RteItem::GetPackageFileName() const
{
  return GetRootFileName();
}

bool RteItem::MatchesHost() const
{
  return MatchesHost(CrossPlatformUtils::GetHostType());
}

bool RteItem::MatchesHost(const string& hostType) const
{
  const string& host = GetAttribute("host");
  return (host.empty() || host == "all" ||
          host == (hostType.empty() ? CrossPlatformUtils::GetHostType() : hostType));
}

RteComponent* RteItem::FindComponents(const RteItem& item, list<RteComponent*>& components) const
{
  for (auto child : GetChildren()) {
    if (child->GetTag() == "bundle" && item.GetCbundleName() == child->GetCbundleName()) {
      return child->FindComponents(item, components);
    }
    RteComponent* c = dynamic_cast<RteComponent*>(child);
    if (c && c->MatchComponent(item)) {
      components.push_back(c);
    }
  }
  return components.empty() ? nullptr :  *(components.begin());
}

bool RteItem::MatchComponent(const RteItem& item) const
{
  if (!item.GetConditionID().empty() && item.GetConditionID() != GetConditionID()) {
    return false;
  }
  return MatchComponentAttributes(item.GetAttributes());
}


bool RteItem::MatchComponentAttributes(const map<string, string>& attributes, bool bRespectVersion) const
{
  if (attributes.empty()) // no limiting attributes
    return true;

  for (auto [ a, v] : attributes) {
    if (a.empty() || a.at(0) != 'C')
      continue; // we are interested only in Cclass, Cgroup, etc.
    auto it = m_attributes.find(a);
    if (it == m_attributes.end()) { // no such attribute in the component
      if (v.empty() || a == "Capiversion")
        continue; // ok: it is required that this attribute is not set or empty
      else
        return false;
    }
    if (a == "Cversion" || a == "Capiversion") { // version of this component should match supplied version range
      int verCompareResult = bRespectVersion? VersionCmp::CompatibleRangeCompare(it->second, v) : 0;
      if (verCompareResult != 0)
        return false;
    } else {
      if (!WildCards::Match(v, it->second))
        return false;
    }
  }
  return true; // no other checks
}


bool RteItem::MatchApiAttributes(const map<string, string>& attributes, bool bRespectVersion) const
{
  if (attributes.empty())
    return false;

  for (auto [a, v] : m_attributes) {
    if (!a.empty() && a[0] == 'C' && a != "Cvendor") {
      auto ita = attributes.find(a);
      if (a == "Capiversion") { // version of this api should be >= supplied version, but less than next major
        if (ita == attributes.end())
          continue; // version is not set => accept any
        if (bRespectVersion && VersionCmp::CompatibleRangeCompare(v, ita->second) != 0) // this version is within supplied range
          return false;
      } else {
        if (ita == attributes.end()) // no api attribute in supplied map
          return false;
        if (!WildCards::Match(v, ita->second))
          return false;
      }
    }
  }
  return true; // no other checks
}


bool RteItem::MatchDeviceAttributes(const map<string, string>& attributes) const
{
  if (attributes.empty())
    return false;

  map<string, string>::const_iterator itm, ita;
  for (auto [a, v] : m_attributes) {
    if (!a.empty() && a[0] == 'D') {
      auto ita = attributes.find(a);
      if (ita == attributes.end()) // no attribute in supplied map
        return false;
      const string& va = ita->second;
      if (a == "Dvendor") {
        if (!DeviceVendor::Match(va, v))
          return false;
      } else if (!WildCards::Match(v, ita->second)) {
        return false;
      }
    }
  }
  return true; // all attributes are found in supplied map
}

bool RteItem::MatchDevice(const map<string, string>& attributes) const
{
  if (attributes.empty())
    return false;

  for (auto [a, v] : m_attributes) {
    if (a == "Dname" || a == "Pname" || a == "Dvendor") {
      auto ita = attributes.find(a);
      if (ita == attributes.end()) // no attribute in supplied map
        return false;
      const string& va = ita->second;
      if (a == "Dvendor") {
        if (!DeviceVendor::Match(va, v))
          return false;
      } else if (!WildCards::Match(v, ita->second)) {
        return false;
      }
    }
  }
  return true; // all attributes are found in supplied map
}

bool RteItem::HasMaxInstances() const
{
  return !GetAttribute("maxInstances").empty();
}

int RteItem::GetMaxInstances() const
{
  int n = GetAttributeAsInt("maxInstances", 1);
  if (n > 1)
    return n;
  return 1; // not specified, single instance : default is always one
}

const string& RteItem::GetDescription() const {
  const string& description = GetItemValue("description");
  if (!description.empty())
    return description;
  return GetText();
}


string RteItem::GetDocFile() const
{
  const string& doc = GetDocValue();
  if (doc.empty()) {
    return doc;
  }
  if (doc.find(":") != string::npos || doc.find("www.") == 0 || doc.find("\\\\") == 0) { // a url or absolute?
    return doc;
  }

  RtePackage* p = GetPackage();
  if (p) {
    return p->GetAbsolutePackagePath() + doc;
  }
  return EMPTY_STRING;
}

bool RteItem::IsReadAccess()
{
  if (HasAttribute("id")) {
    return true;
  }
  const string& access = GetAccess();
  return access.empty() || access.find('r') != string::npos;
}

bool RteItem::IsWriteAccess()
{
  const string& id = GetAttribute("id");
  if (!id.empty()) {
    return id.find("IRAM") == 0;
  }
  const string& access = GetAccess();
  return access.find('w') != string::npos;
}

bool RteItem::IsExecuteAccess()
{
  const string& id = GetAttribute("id");
  if (!id.empty()) {
    return id.find("IROM") == 0;
  }
  const string& access = GetAccess();
  return access.find('x') != string::npos;
}

bool RteItem::IsSecureAccess()
{
  if (HasAttribute("id")) {
    return true;
  }
  const string& access = GetAccess();
  return access.find('s') != string::npos && access.find('n') == string::npos;
}
bool RteItem::IsNonSecureAccess()
{
  if (HasAttribute("id")) {
    return false;
  }
  const string& access = GetAccess();
  return access.find('n') != string::npos && access.find('s') == string::npos;
}

bool RteItem::IsCallableAccess()
{
  if (HasAttribute("id")) {
    return false;
  }
  const string& access = GetAccess();
  return access.find('c') != string::npos;
}

bool RteItem::IsPeripheralAccess()
{
  if (HasAttribute("id")) {
    return false;
  }
  const string& access = GetAccess();
  return access.find('p') != string::npos;
}


string RteItem::GetOriginalAbsolutePath() const
{
  return GetOriginalAbsolutePath(GetName());
}

string RteItem::GetOriginalAbsolutePath(const string& name) const
{
  if (name.empty() || name.find(":") != string::npos || name.find("www.") == 0 || name.find("\\\\") == 0) { // a url or absolute?
    return name;
  }
  RtePackage* p = GetPackage();
  string absPath;
  if (p) {
    absPath = p->GetAbsolutePackagePath();
  }
  absPath += name;
  return RteFsUtils::MakePathCanonical(absPath);
}

string RteItem::ExpandString(const string& str, bool bUseAccessSequences, RteItem* context) const
{
  if (str.empty())
    return str;
  if(context && context != this) {
    return context->ExpandString(str, bUseAccessSequences, context);
  }
  RteCallback* pCallback = GetCallback();
  if (pCallback)
    return pCallback->ExpandString(str);
  return str;
}

string RteItem::GetDownloadUrl(bool withVersion, const char* extension) const
{
  string url = EMPTY_STRING;
  RtePackage* package = GetPackage();
  if (package && package != this) {
    url = package->GetDownloadUrl(withVersion, extension);
  }
  return url;
}

RteCondition* RteItem::GetCondition() const
{
  return GetCondition(GetConditionID());
}

RteCondition* RteItem::GetCondition(const string& id) const
{
  if (m_parent && !id.empty())
    return m_parent->GetCondition(id);
  return nullptr;
}

RteItem* RteItem::GetLicenseSet() const
{
  RtePackage* pack = GetPackage();
  if(pack) {
    return pack->GetLicenseSet(GetAttribute("licenseSet"));
  }
  return nullptr;
}

RteItem* RteItem::GetDefaultChild() const
{
  for (auto item : GetChildren()) {
    if (item->IsDefault()) {
      return item;
    }
  }
  return nullptr;
}

bool RteItem::IsDeviceDependent() const
{
  RteCondition *condition = GetCondition();
  return condition && condition->IsDeviceDependent();
}

bool RteItem::IsBoardDependent() const
{
  RteCondition* condition = GetCondition();
  return condition && condition->IsBoardDependent();
}

RteItem::ConditionResult RteItem::Evaluate(RteConditionContext* context)
{
  RteCondition* condition = GetCondition();
  if (condition) {
    if (context->IsVerbose()) {
      GetCallback()->OutputMessage(GetID());
    }
    return context->Evaluate(condition);
  }
  return IGNORED;
}

RteItem::ConditionResult RteItem::GetConditionResult(RteConditionContext* context) const
{
  RteCondition* condition = GetCondition();
  if (condition)
    return context->GetConditionResult(condition);
  return IGNORED;
}


RteItem::ConditionResult RteItem::GetDepsResult(map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const
{
  ConditionResult r = RteDependencyResult::GetResult(this, results);
  if (r != UNDEFINED)
    return r;
  ConditionResult result = GetConditionResult(target->GetDependencySolver());
  if (result < FULFILLED && result > FAILED && result != CONFLICT) { // CONFLICT is reported by corresponding API
    RteCondition* condition = GetCondition();
    if (condition) {
      RteDependencyResult depRes(this, result);
      result = condition->GetDepsResult(depRes.Results(), target);
      results[this] = depRes;
    }
  }
  return result;
}


void RteItem::Construct()
{
  m_ID = ConstructID();
}


XMLTreeElement* RteItem::CreateXmlTreeElement(XMLTreeElement* parentElement, bool bCreateContent) const
{
  XMLTreeElement* element = new XMLTreeElement(parentElement);
  element->SetTag(GetTag());
  element->SetAttributes(GetAttributes());
  const string &text = GetText();
  if (bCreateContent) {
    CreateXmlTreeElementContent(element);
  }
  if (!text.empty()) {
    if (GetChildCount() > 0) {
      element->CreateElement("description", text);
    } else {
      element->SetText(text);
    }
  }
  return element;
}

void RteItem::CreateXmlTreeElementContent(XMLTreeElement* parentElement) const
{
  for (auto item : m_children) {
    item->CreateXmlTreeElement(parentElement);
  }
}

RteItem* RteItem::CreateChild(const string& tag, const string& name)
{
  RteItem* item = new RteItem(this);
  AddItem(item);
  item->SetTag(tag);
  if (!name.empty()) {
    item->AddAttribute("name", name);
  }
  return item;
}

bool RteItem::Validate()
{
  m_bValid = true; // assume valid
  const string& conditionId = GetConditionID();
  if (!conditionId.empty()) {
    if (!GetCondition(conditionId) && GetPackage()) {
      string msg = " condition '";
      msg += conditionId;
      msg += "' not found";
      m_errors.push_back(msg);
      m_bValid = false;
    }
  }
  for (auto item : m_children) {
    if (!item->Validate())
      m_bValid = false;
  }
  return m_bValid;
}

void RteItem::InsertInModel(RteModel* model)
{
  // default just iterates over children asking them to insert themselves
  for (auto item : m_children) {
    item->InsertInModel(model);
  }
}

bool RteItem::HasXmlContent() const
{
  return GetChildCount() > 0;
}

bool RteItem::CompareComponents(RteItem* c0, RteItem* c1)
{
  int res = AlnumCmp::Compare(c0->GetCbundleName(), c1->GetCbundleName());
  if (!res)
    res = AlnumCmp::Compare(c0->GetCclassName(), c1->GetCclassName());
  if (!res)
    res = AlnumCmp::Compare(c0->GetCgroupName(), c1->GetCgroupName());
  if (!res)
    res = AlnumCmp::Compare(c0->GetCsubName(), c1->GetCsubName());
  if (!res)
    res = AlnumCmp::Compare(c0->GetCvariantName(), c1->GetCvariantName());
  if (!res)
    res = AlnumCmp::Compare(c0->GetVendorString(), c1->GetVendorString());
  if (!res)
    res = AlnumCmp::Compare(c0->GetComponentUniqueID(), c1->GetComponentUniqueID());
  return res < 0;
}

void RteItem::SortChildren(CompareRteItemType cmp)
{
  m_children.sort(cmp);
}

RteItem* RteRootItem::CreateItem(const std::string& tag)
{
  // for YAML files create corresponding items depending on the root itself
  if(GetTag() == "generator") {
    return new RteGenerator(this, true);
  }
  return RteItem::CreateItem(tag);
}

// End of RteItem.cpp
