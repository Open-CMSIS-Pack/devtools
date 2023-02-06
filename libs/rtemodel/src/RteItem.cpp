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

#include "RteFsUtils.h"
#include "XMLTree.h"
#include "CrossPlatformUtils.h"

using namespace std;

RteItem::RteItem(RteItem* parent) :
  m_parent(parent),
  m_bValid(false)
{
}

RteItem::RteItem(const std::map<std::string, std::string>& attributes, RteItem* parent) :
  XmlItem(attributes),
  m_parent(parent),
  m_bValid(true)
{
};

RteItem::~RteItem()
{
  RteItem::Clear(); // must be so: explicit call to this class implementation!
}


void RteItem::Clear()
{
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    (*it)->Clear();
    delete *it;
  }
  m_children.clear();
  m_bValid = false;
  m_errors.clear();
  XmlItem::Clear();
}


void RteItem::Reparent(RteItem* newParent)
{
  if (m_parent == newParent)
    return;
  if (m_parent) {
    m_parent->RemoveItem(this);
  }
  if (newParent) {
    newParent->AddItem(this);
  }
  m_parent = newParent;
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

string RteItem::GetComponentUniqueID(bool withVersion) const
{
  RteComponent* c = GetComponent();
  if (c && c != this) {
    return c->GetComponentUniqueID(withVersion);
  }
  if (IsApi()) {
    withVersion = false; // api ID is always without version (the latest is used)
    return GetApiID(false);
  }
  return ConstructComponentID(GetVendorAndBundle(), true, withVersion, true);
}

string RteItem::GetComponentID(bool withVersion) const // to use in filtered list
{
  if (IsApi()) {
    withVersion = false; // api ID is always without version (the latest is used)
    return GetApiID(false);
  }
  return ConstructComponentID(GetVendorAndBundle(), true, withVersion, false);
}

string RteItem::GetComponentAggregateID() const
{
  RteComponent* c = GetComponent();
  if (c && c != this) {
    return c->GetComponentAggregateID();
  }
  return ConstructComponentID(GetVendorAndBundle(), false, false, false);
}

string RteItem::GetApiID(bool withVersion) const
{
  string id = "::";
  id += GetCclassName();
  id += ".";
  id += GetCgroupName();

  if (withVersion) {
    const string& ver = GetAttribute("Capiversion");
    if (!ver.empty()) {
      id += ":";
      id += ver;
    }
  }
  id += "(API)";
  return id;
}

string RteItem::GetVendorAndBundle() const
{
  string s;
  if (!IsApi()) {
    s = GetVendorName();
    if (!GetCbundleName().empty()) {
      s += ".";
      s += GetCbundleName();
    }
  }
  return s;
};

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

string RteItem::ConstructComponentID(const string& prefix, bool bVariant, bool bVersion, bool bCondition, char delimiter) const
{
  string id = prefix;
  id += "::";
  id += ConcatenateCclassCgroupCsub(delimiter);

  if (bCondition && !GetConditionID().empty())
  {
    id += "(";
    id += GetConditionID();
    id += ")";
  }

  // optional variant
  if (bVariant && !GetCvariantName().empty()) {
    id += ":";
    id += GetCvariantName();
  }

  if (bVersion) {
    const string& ver = GetVersionString();
    if (!ver.empty()) {
      id += ":";
      id += ver;
    }
  }
  return id;
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
    id += ":";
    id += GetCvariantName();
  }

  if (bVersion) {
    const string& ver = GetVersionString();
    if (!ver.empty()) {
      id += ":";
      id += ver;
    }
  }
  return id;
}


const list<RteItem*>& RteItem::GetItemChildren(RteItem* item)
{
  if (item && item->GetChildCount() > 0)
    return item->GetChildren();

  static const list<RteItem*> EMPTY_ITEM_LIST;
  return EMPTY_ITEM_LIST;
}

const list<RteItem*>& RteItem::GetGrandChildren(const string& tag) const
{
  RteItem* child = GetItemByTag(tag);
  return GetItemChildren(child);
}

const list<RteItem*>& RteItem::GetItemGrandChildren(RteItem* item, const string& tag)
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

list<RteItem*>& RteItem::GetChildrenByTag(const std::string& tag, list<RteItem*>& items) const
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
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    RteItem* item = *it;
    if (item->GetID() == id)
      return item;
  }
  return nullptr;
}

bool RteItem::HasItem(RteItem* item) const
{
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    if (item == *it)
      return true;
  }
  return false;
}

RteItem* RteItem::GetItemByTag(const string& tag) const
{
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    RteItem* item = *it;
    if (item->GetTag() == tag)
      return item;
  }
  return nullptr;

}

const string& RteItem::GetChildAttribute(const std::string& tag, const std::string& attribute) const
{
  RteItem* child = GetItemByTag(tag);
  if (child) {
    return child->GetAttribute(attribute);
  }
  return EMPTY_STRING;
}

const string& RteItem::GetChildText(const string& tag) const
{
  RteItem* child = GetItemByTag(tag);
  if (child)
    return child->GetText();
  return EMPTY_STRING;
}

const string& RteItem::GetItemValue(const string& nameOrTag) const
{
  if (HasAttribute(nameOrTag)) {
    return GetAttribute(nameOrTag);
  }
  return GetChildText(nameOrTag);
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
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    if (item == *it) {
      m_children.erase(it);
      break;
    }
  }
}

string RteItem::ConstructID()
{
  string id = GetName();
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

string RteItem::GetProjectGroupName() const
{
  return string("::") + GetCclassName();
}

string RteItem::GetBundleShortID() const
{
  string s;
  if (!GetCbundleName().empty()) {
    s = GetVendorAndBundle();
  }
  return s;
};

string RteItem::GetBundleID(bool bWithVersion) const
{
  if (!GetCbundleName().empty()) {
    string s = GetVendorAndBundle();
    s += "::";
    s += GetCclassName();
    if (bWithVersion && !GetVersionString().empty()) {
      s += ":";
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
      taxonomyId += "." + group;
      const string& sub = attributes.GetAttribute("Csub");
      if (!sub.empty()) {
        taxonomyId += "." + sub;
      }
    }
  }
  return taxonomyId;
}

string RteItem::GetPackageID(bool withVersion) const
{
  RtePackage* package = GetPackage();
  if (package == this) {
    return RtePackage::GetPackageIDfromAttributes(*this, withVersion);
  } else if (package) {
    return package->GetPackageID(withVersion);
  }
  return EMPTY_STRING;
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
  return PS_UNKNOWN;

}

const string& RteItem::GetPackageFileName() const
{
  if (m_parent)
    return m_parent->GetPackageFileName();
  return EMPTY_STRING;
}

bool RteItem::MatchesHost() const
{
  const string& host = GetAttribute("host");
  if (host.empty() || host == "all" || host == CrossPlatformUtils::GetHostType())
    return true;
  return false;
}


bool RteItem::MatchComponentAttributes(const map<string, string>& attributes) const
{
  if (attributes.empty()) // no limiting attributes
    return true;

  map<string, string>::const_iterator it, ita;
  for (ita = attributes.begin(); ita != attributes.end(); ita++) {
    const string& a = ita->first;
    const string& v = ita->second;
    if (a.empty() || a.at(0) != 'C')
      continue; // we are interested only in Cclass, Cgroup, etc.
    it = m_attributes.find(a);
    if (it == m_attributes.end()) { // no such attribute in the component
      if (v.empty() || a == "Capiversion")
        continue; // ok: it is required that this attribute is not set or empty
      else
        return false;
    }
    if (a == "Cversion" || a == "Capiversion") { // version of this component should match supplied version range
      int verCompareResult = VersionCmp::RangeCompare(it->second, v);
      if (verCompareResult != 0)
        return false;
    } else {
      if (!WildCards::Match(v, it->second))
        return false;
    }
  }
  return true; // no other checks
}


bool RteItem::MatchApiAttributes(const map<string, string>& attributes) const
{
  if (attributes.empty())
    return false;

  map<string, string>::const_iterator itm, ita;
  for (itm = m_attributes.begin(); itm != m_attributes.end(); itm++) {
    const string& a = itm->first;
    if (!a.empty() && a[0] == 'C' && a != "Cvendor") {
      const string& v = itm->second;
      ita = attributes.find(a);
      if (a == "Capiversion") { // version of this api should be >= supplied version
        if (ita == attributes.end())
          continue; // version is not set => accept any
        if (VersionCmp::RangeCompare(v, ita->second) != 0) // this version is within supplied range
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
  for (itm = m_attributes.begin(); itm != m_attributes.end(); itm++) {
    const string& a = itm->first;
    if (!a.empty() && a[0] == 'D') {
      const string& v = itm->second;
      ita = attributes.find(a);
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

  map<string, string>::const_iterator itm, ita;
  for (itm = m_attributes.begin(); itm != m_attributes.end(); itm++) {
    const string& a = itm->first;
    if (a == "Dname" || a == "Pname" || a == "Dvendor") {
      const string& v = itm->second;
      ita = attributes.find(a);
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

string RteItem::ExpandString(const string& str) const
{
  if (str.empty())
    return str;

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
  if (condition)
    return context->Evaluate(condition);
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


bool RteItem::Construct(XMLTreeElement* xmlElement)
{
  if (!xmlElement)
    return false;
  // set data to this item
  m_lineNumber = xmlElement->GetLineNumber();
  m_tag = xmlElement->GetTag();
  m_text = xmlElement->GetText();
  SetAttributes(xmlElement->GetAttributes());
// process children
  bool success = ProcessXmlChildren(xmlElement);
// create id after processing children, it might be a child that contains required information, e.g. RtePackage
  m_ID = ConstructID();
  return success;
}


bool RteItem::ProcessXmlChildren(XMLTreeElement* xmlElement)
{
  if (!xmlElement)
    return false;
  const list<XMLTreeElement*>& children = xmlElement->GetChildren();
  list<XMLTreeElement*>::const_iterator it = children.begin();
  for (; it != children.end(); it++) {
    if (!ProcessXmlElement(*it))
      return false;
  }
  return true;
}


bool RteItem::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  const string& name = xmlElement->GetTag();
  if (name == "description") {
    m_text = xmlElement->GetText(); // if an item has children, it cannot have also a text, so reuse the member
    return true;
  }
  RteItem* child = new RteItem(this);
  if (child->Construct(xmlElement)) {
    AddItem(child);
    return true;
  }
  delete child;
  return false;
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
  if (GetChildCount() > 0) {
    element->CreateElement("description", text, false);
  }
  else {
    element->SetText(text);
  }
  return element;
}

void RteItem::CreateXmlTreeElementContent(XMLTreeElement* parentElement) const
{
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    RteItem* item = *it;
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
    if (!GetCondition(conditionId)) {
      string msg = " condition '";
      msg += conditionId;
      msg += "' not found";
      m_errors.push_back(msg);
      m_bValid = false;
    }
  }
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    if (!(*it)->Validate())
      m_bValid = false;
  }
  return m_bValid;
}

void RteItem::InsertInModel(RteModel* model)
{
  // default just iterates over children asking them to insert themselves
  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    (*it)->InsertInModel(model);
  }
}

bool RteItem::AcceptVisitor(RteVisitor* visitor)
{
  // visitor design pattern implementation
  if (visitor) {
    VISIT_RESULT res = visitor->Visit(this);
    if (res == CANCEL_VISIT) {
      return false;
    }
    if (res == CONTINUE_VISIT) {
      for (auto it = m_children.begin(); it != m_children.end(); it++) {
        if (!(*it)->AcceptVisitor(visitor))
          return false;
      }
    }
    return true;
  }
  return false;
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
    res = AlnumCmp::Compare(c0->GetComponentUniqueID(true), c1->GetComponentUniqueID(true));
  return res < 0;
}

void RteItem::SortChildren(CompareRteItemType cmp)
{
  m_children.sort(cmp);
}

RteItemContainer::RteItemContainer(RteItem* parent) :
  RteItem(parent)
{
}

bool RteItemContainer::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  RteItem* child = new RteItem(this);
  if (child->Construct(xmlElement)) {
    AddItem(child);
  } else {
    delete child;
    return false;
  }
  return true;
}

// End of RteItem.cpp
