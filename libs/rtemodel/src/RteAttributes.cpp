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
const RteAttributes RteAttributes::EMPTY_ATTRIBUTES;

RteAttributes::RteAttributes(const map<string, string>& attributes) :
  XmlItem(attributes)
{
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
  if (GetAttributeCount() != other.GetAttributeCount())
    return false;
  return CompareAttributes(other.GetAttributes());
}

bool RteAttributes::Compare(const RteAttributes* other) const
{
  if (!other || GetAttributeCount() != other->GetAttributeCount())
    return false;
  return CompareAttributes(other->GetAttributes());
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
