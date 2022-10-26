/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteAttributes.cpp
* @brief CMSIS RTE Data model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteAttributes.h"

#include "RteUtils.h"

#include <sstream>

using namespace std;

///////////////////////////
const string RteAttributes::EMPTY_STRING("");

RteAttributes::RteAttributes(const map<string, string>& attributes) :
  m_attributes(attributes)
{
}

RteAttributes::~RteAttributes()
{
  m_attributes.clear();
};

const string& RteAttributes::GetName() const
{
  const string& name = GetAttribute("name");
  if (name.empty())
    return m_tag;
  return name;
}


const string& RteAttributes::GetAttribute(const string& name) const
{
  map<string, string>::const_iterator it = m_attributes.find(name);
  if (it != m_attributes.end())
    return it->second;
  return EMPTY_STRING;
}

bool RteAttributes::HasAttribute(const string& name) const
{
  map<string, string>::const_iterator it = m_attributes.find(name);
  return it != m_attributes.end();
}

bool RteAttributes::HasValue(const string& pattern) const
{
  for (auto itm = m_attributes.begin(); itm != m_attributes.end(); itm++) {
    if (WildCards::Match(pattern, itm->second))
      return true;
  }
  return false;
}


bool RteAttributes::CompareAttributes(const map<string, string>& attributes) const
{
  // all supplied attributes must exist in this ones
  map<string, string>::const_iterator itm, ita;
  for (ita = attributes.begin(); ita != attributes.end(); ita++) {
    const string& a = ita->first;
    const string& v = ita->second;
    itm = m_attributes.find(a);
    if (itm != m_attributes.end()) {
      const string& va = itm->second;
      if (a == "Dvendor" || a == "vendor") {
        if (!DeviceVendor::Match(va, v))
          return false;
      } else if (!WildCards::Match(va, v)) {
        return false;
      }
    } else {
      return false;
    }
  }
  return true;
}

bool RteAttributes::Compare(const RteAttributes& other) const
{
  if (GetCount() != other.GetCount())
    return false;
  return CompareAttributes(other.GetAttributes());
}

bool RteAttributes::Compare(const RteAttributes* other) const
{
  if (!other || GetCount() != other->GetCount())
    return false;
  return CompareAttributes(other->GetAttributes());
}


bool RteAttributes::EqualAttributes(const map<string, string>& attributes) const
{
  // all supplied attributes must exist in this ones
  map<string, string>::const_iterator itm, ita;
  for (ita = attributes.begin(); ita != attributes.end(); ita++) {
    const string& a = ita->first;
    const string& v = ita->second;
    itm = m_attributes.find(a);
    if (itm != m_attributes.end()) {
      const string& va = itm->second;
      if (va != v)
        return false;
    } else {
      return false;
    }
  }
  return true;
}

bool RteAttributes::EqualAttributes(const RteAttributes& other) const
{
  if (GetCount() != other.GetCount())
    return false;
  return EqualAttributes(other.GetAttributes());
}

bool RteAttributes::EqualAttributes(const RteAttributes* other) const
{
  if (!other || GetCount() != other->GetCount())
    return false;
  return EqualAttributes(other->GetAttributes());
}


bool RteAttributes::GetAttributeAsBool(const char* name, bool defaultValue) const
{
  const string& s = GetAttribute(name);
  if (s.empty())
    return defaultValue;
  return (s == "1" || s == "true");
}

int RteAttributes::GetAttributeAsInt(const char* name, int defaultValue) const
{
  const string& s = GetAttribute(name);
  if (s.empty())
    return defaultValue;
  return stoi(s, 0, 0);
}

unsigned RteAttributes::GetAttributeAsUnsigned(const char* name, unsigned defaultValue) const
{
  const string& s = GetAttribute(name);
  if (s.empty())
    return defaultValue;
  return RteUtils::ToUL(s);
}

unsigned long long RteAttributes::GetAttributeAsULL(const char* name, unsigned long long defaultValue) const {
  const string& s = GetAttribute(name);

  if (s.empty()) {
    return (defaultValue);
  }
  return RteUtils::ToULL(s);
}


string RteAttributes::GetAttributesString() const
{
  string s;
  map<string, string>::const_iterator it;
  for (it = m_attributes.begin(); it != m_attributes.end(); it++) {
    if (!s.empty())
      s += " ";
    s += it->first;
    s += "=";
    s += it->second;
  }
  return s;
}

string RteAttributes::GetAttributesAsXmlString() const
{
  return RteUtils::ToXmlString(m_attributes);
}


bool RteAttributes::SetAttribute(const char* name, const char* value)
{
  if (!name)
    return false;
  map<string, string>::iterator it = m_attributes.find(name);
  if (it != m_attributes.end()) {
    if (value && it->second == value)
      return false;
    m_attributes.erase(it);
  }
  if (value) {
    m_attributes[name] = value;
  }
  return true;
}

bool RteAttributes::SetAttribute(const char* name, long value, int radix)
{
  ostringstream ss;
  if (radix == 16) {
    ss << showbase << hex;
  }
  ss << value;
  const string s = ss.str();
  const char* c = s.c_str();
  return SetAttribute(name, c);
}

bool RteAttributes::AddAttribute(const string& name, const string& value, bool insertEmpty)
{
  if (name.empty())
    return false;
  map<string, string>::iterator it = m_attributes.find(name);
  if (it != m_attributes.end()) {
    if (it->second == value)
      return false;
    if (!insertEmpty && value.empty()) {
      m_attributes.erase(it);
      return true;
    }
  }
  if (insertEmpty || !value.empty())
    m_attributes[name] = value;
  return true;
}


bool RteAttributes::SetAttributes(const map<string, string>& attributes)
{
  if (m_attributes == attributes)
    return false;

  m_attributes = attributes;
  ProcessAttributes();
  return true;
}

bool RteAttributes::SetAttributes(const RteAttributes& attributes)
{
  return SetAttributes(attributes.GetAttributes());
}


bool RteAttributes::AddAttributes(const map<string, string>& attributes, bool replaceExisting)
{
  if (attributes.empty())
    return false;
  bool bChanged = false;
  if (m_attributes.empty()) {
    bChanged = true;
    SetAttributes(attributes);
  } else {
    for (auto it = attributes.begin(); it != attributes.end(); it++) {
      if (replaceExisting || !HasAttribute(it->first)) {
        if (AddAttribute(it->first, it->second))
          bChanged = true;
      }
    }
  }
  if (bChanged)
    ProcessAttributes();
  return bChanged;
}


bool RteAttributes::RemoveAttribute(const char* name)
{
  if (name) {
    map<string, string>::iterator it = m_attributes.find(name);
    if (it != m_attributes.end()) {
      m_attributes.erase(it);
      return true;
    }
  }
  return false;
}


bool RteAttributes::ClearAttributes()
{
  if (m_attributes.empty())
    return false;
  m_attributes.clear();
  ProcessAttributes();
  return true;
}


string RteAttributes::GetAttributePrefix(const char* name, char delimiter) const
{
  return RteUtils::GetPrefix(GetAttribute(name), delimiter);
}

string RteAttributes::GetAttributeSuffix(const char* name, char delimiter) const
{
  return RteUtils::GetSuffix(GetAttribute(name), delimiter);
}


int RteAttributes::GetAttributeSuffixAsInt(const char* name, char delimiter) const
{
  return RteUtils::GetSuffixAsInt(GetAttribute(name), delimiter);
}


const string& RteAttributes::GetVendorString() const
{
  if (IsApi())
    return EMPTY_STRING;
  const string& cv = GetAttribute("Cvendor");
  if (cv.empty())
    return GetAttribute("vendor");
  return cv;
}

string RteAttributes::GetVendorName() const
{
  const string& vendor = GetVendorString();
  return DeviceVendor::GetCanonicalVendorName(vendor);
};


string RteAttributes::GetVendorAndBundle() const
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

string RteAttributes::GetBundleShortID() const
{
  string s;
  if (!GetCbundleName().empty()) {
    s = GetVendorAndBundle();
  }
  return s;
};


string RteAttributes::GetBundleID(bool bWithVersion) const
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

string RteAttributes::GetTaxonomyDescriptionID() const
{
  return GetTaxonomyDescriptionID(GetAttributes());
}

string RteAttributes::GetTaxonomyDescriptionID(const map<string, string>& attributes)
{
  string s;
  auto it = attributes.find("Cclass");
  if (it != attributes.end()) {
    s = it->second;
  }
  if (!s.empty()) { // taxonomy must have at least Cclass attribute
    it = attributes.find("Cgroup");
    if (it != attributes.end() && !it->second.empty()) {
      s += ".";
      s += it->second;
      it = attributes.find("Csub");
      if (it != attributes.end() && !it->second.empty()) {
        s += ".";
        s += it->second;
      }
    }
  }
  return s;
}

string RteAttributes::GetPackageID(bool withVersion) const
{
  return GetPackageIDfromAttributes(*this, withVersion);
}

string RteAttributes::GetPackageIDfromAttributes(const RteAttributes& attr, bool withVersion)
{

  string id = attr.GetAttribute("vendor");
  if (!id.empty())
    id += ".";
  id += attr.GetAttribute("name");

  if (withVersion && !attr.GetVersionString().empty()) {
    id += ".";
    id += VersionCmp::RemoveVersionMeta(attr.GetVersionString());
  }
  return id;
}


const string& RteAttributes::GetVersionString() const
{
  if (IsApi())
    return GetApiVersionString();
  const string& ver = GetAttribute("Cversion");
  if (!ver.empty())
    return ver;
  return GetAttribute("version");
}

string RteAttributes::GetVersionVariantString() const
{
  string s = GetVersionString();
  const string& variant = GetCvariantName();
  if (!variant.empty()) {
    s += " ";
    s += variant;
  }
  return s;
}


const string& RteAttributes::GetDocAttribute() const
{
  const string& doc = GetAttribute("doc");
  if (!doc.empty())
    return doc;
  return GetAttribute("name");
}

string RteAttributes::GetFullDeviceName() const
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

bool RteAttributes::IsReadAccess()
{
  if (HasAttribute("id")) {
    return true;
  }
  const string& access = GetAccess();
  return access.empty() || access.find('r') != string::npos;
}

bool RteAttributes::IsWriteAccess()
{
  const string& id = GetAttribute("id");
  if (!id.empty()) {
    return id.find("IRAM") == 0;
  }
  const string& access = GetAccess();
  return access.find('w') != string::npos;
}

bool RteAttributes::IsExecuteAccess()
{
  const string& id = GetAttribute("id");
  if (!id.empty()) {
    return id.find("IROM") == 0;
  }
  const string& access = GetAccess();
  return access.find('x') != string::npos;
}

bool RteAttributes::IsSecureAccess()
{
  if (HasAttribute("id")) {
    return true;
  }
  const string& access = GetAccess();
  return access.find('s') != string::npos && access.find('n') == string::npos;
}
bool RteAttributes::IsNonSecureAccess()
{
  if (HasAttribute("id")) {
    return false;
  }
  const string& access = GetAccess();
  return access.find('n') != string::npos && access.find('s') == string::npos;
}

bool RteAttributes::IsCallableAccess()
{
  if (HasAttribute("id")) {
    return false;
  }
  const string& access = GetAccess();
  return access.find('c') != string::npos;
}

bool RteAttributes::IsPeripheralAccess()
{
  if (HasAttribute("id")) {
    return false;
  }
  const string& access = GetAccess();
  return access.find('p') != string::npos;
}



string RteAttributes::GetProjectGroupName() const
{
  string id = "::";
  id += GetCclassName();
  return id;
}

string RteAttributes::GetComponentUniqueID(bool withVersion) const // full unique component ID
{
  if (IsApi()) {
    withVersion = false; // api ID is always without version (the latest is used)
    return GetApiID(false);
  }
  return ConstructComponentID(GetVendorAndBundle(), true, withVersion, true);
}

string RteAttributes::GetComponentID(bool withVersion) const // to use in filtered list
{
  if (IsApi()) {
    withVersion = false; // api ID is always without version (the latest is used)
    return GetApiID(false);
  }
  return ConstructComponentID(GetVendorAndBundle(), true, withVersion, false);
}

string RteAttributes::GetApiID(bool withVersion) const
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

string RteAttributes::GetComponentAggregateID() const
{
  return ConstructComponentID(GetVendorAndBundle(), false, false, false);
}

string RteAttributes::ConcatenateCclassCgroupCsub(char delimiter) const
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

string RteAttributes::ConstructComponentID(const string& prefix, bool bVariant, bool bVersion, bool bCondition, char delimiter) const
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

string RteAttributes::ConstructComponentDisplayName(bool bClass, bool bVariant, bool bVersion, char delimiter) const
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

string RteAttributes::ConstructComponentPreIncludeFileName() const
{
  string fileName = "Pre_Include_";
  return fileName + WildCards::ToX(ConcatenateCclassCgroupCsub('_')) + ".h";
}

bool RteAttributes::HasComponentAttributes(const map<string, string>& attributes) const
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

bool RteAttributes::MatchApiAttributes(const map<string, string>& attributes) const
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

bool RteAttributes::MatchDeviceAttributes(const map<string, string>& attributes) const
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

bool RteAttributes::MatchDevice(const map<string, string>& attributes) const
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

bool RteAttributes::HasMaxInstances() const
{
  return !GetAttribute("maxInstances").empty();
}

int RteAttributes::GetMaxInstances() const
{
  int n = GetAttributeAsInt("maxInstances", 1);
  if (n > 1)
    return n;
  return 1; // not specified, single instance : default is always one
}

// End of RteAttributes.cpp
