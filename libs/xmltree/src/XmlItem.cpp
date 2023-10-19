/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "XmlItem.h"

#include "RteUtils.h"

using namespace std;

const string XmlItem::EMPTY_STRING("");

void XmlItem::Clear()
{
  m_attributes.clear();
}

void XmlItem::ClearAttributes()
{
  if (!m_attributes.empty()) {
    m_attributes.clear();
    ProcessAttributes();
  }
}

bool XmlItem::AddAttributes(const map<string, string>& attributes, bool replaceExisting)
{
  if (attributes.empty())
    return false;
  bool bChanged = false;
  if (m_attributes.empty()) {
    bChanged = true;
    SetAttributes(attributes);
  } else {
    for (auto [a, v] : attributes) {
      if (replaceExisting || !HasAttribute(a)) {
        if (AddAttribute(a, v))
          bChanged = true;
      }
    }
  }
  if (bChanged) {
    ProcessAttributes();
  }
  return bChanged;
}

bool XmlItem::AddAttribute(const string& name, const string& value, bool insertEmpty)
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

bool XmlItem::SetAttribute(const char* name, const char* value)
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

bool XmlItem::SetAttribute(const char* name, long value, int radix)
{
  return SetAttribute(name, RteUtils::LongToString(value, radix).c_str());
}

bool XmlItem::SetAttributes(const map<string, string>& attributes)
{
  if (m_attributes == attributes)
    return false;

  m_attributes = attributes;
  ProcessAttributes();
  return true;
}


bool XmlItem::SetAttributes(const XmlItem& attributes)
{
  return SetAttributes(attributes.GetAttributes());
}

bool XmlItem::RemoveAttribute(const std::string& name)
{
  map<string, string>::iterator it = m_attributes.find(name);
  if (it != m_attributes.end()) {
    m_attributes.erase(it);
    return true;
  }
  return false;
}

bool XmlItem::RemoveAttribute(const char* name)
{
  if (name) {
    return RemoveAttribute(string(name));
  }
  return false;
}

bool XmlItem::EraseAttributes(const std::string& pattern)
{
  set<string> attributesToErase;
  for ( auto [key, _] : m_attributes) {
    if (WildCards::Match(pattern, key)) {
      attributesToErase.insert(key);
    }
  }
  if (attributesToErase.empty()) {
    return false;
  }
  for ( auto& key : attributesToErase) {
    RemoveAttribute(key);
  }
  return true;
}

const string& XmlItem::GetAttribute(const char* name) const
{
  if (name)
    return GetAttribute(string(name));
  return EMPTY_STRING;
}

const string& XmlItem::GetAttribute(const string& name) const
{
  map<string, string>::const_iterator it = m_attributes.find(name);
  if (it != m_attributes.end())
    return it->second;
  return EMPTY_STRING;
}

bool XmlItem::HasAttribute(const char* name) const
{
  if (name)
    return HasAttribute(string(name));
  return false;
}

bool XmlItem::HasAttribute(const std::string& name) const
{
  return m_attributes.find(name) != m_attributes.end();
}


bool XmlItem::HasValue(const string& pattern) const
{
  for (auto [a, v] : m_attributes) {
    if (WildCards::Match(pattern, v))
      return true;
  }
  return false;
}


const string& XmlItem::GetName() const
{
  const string& name = GetAttribute("name");
  if (!name.empty())
    return name;
  return m_tag;
}


bool XmlItem::GetAttributeAsBool(const char* name, bool defaultValue) const
{
  if (name)
    return GetAttributeAsBool(string(name), defaultValue);
  return defaultValue;
}

int XmlItem::GetAttributeAsInt(const char* name, int defaultValue) const
{
  if (name)
    return GetAttributeAsInt(string(name), defaultValue);
  return defaultValue;
}

unsigned XmlItem::GetAttributeAsUnsigned(const char* name, unsigned defaultValue) const
{
  if (name)
    return GetAttributeAsUnsigned(string(name), defaultValue);
  return defaultValue;
}

unsigned long long XmlItem::GetAttributeAsULL(const char* name, unsigned long long defaultValue) const
{
  if (name)
    return GetAttributeAsULL(string(name), defaultValue);
  return defaultValue;
}


bool XmlItem::GetAttributeAsBool(const string& name, bool defaultValue) const
{
  return RteUtils::StringToBool(GetAttribute(name), defaultValue);
}

int XmlItem::GetAttributeAsInt(const string& name, int defaultValue) const
{
  return RteUtils::StringToInt(GetAttribute(name), defaultValue);
}

unsigned XmlItem::GetAttributeAsUnsigned(const string& name, unsigned defaultValue) const
{
  return RteUtils::StringToUnsigned(GetAttribute(name), defaultValue);
}

unsigned long long XmlItem::GetAttributeAsULL(const string& name, unsigned long long defaultValue) const
{
  return RteUtils::StringToULL(GetAttribute(name), defaultValue);
}

string XmlItem::GetAttributePrefix(const char* name, char delimiter) const
{
  return RteUtils::GetPrefix(GetAttribute(name), delimiter);
}

string XmlItem::GetAttributeSuffix(const char* name, char delimiter) const
{
  return RteUtils::GetSuffix(GetAttribute(name), delimiter);
}

int XmlItem::GetAttributeSuffixAsInt(const char* name, char delimiter) const
{
  return RteUtils::GetSuffixAsInt(GetAttribute(name), delimiter);
}

bool  XmlItem::GetTextAsBool(bool defaultValue) const
{
  return RteUtils::StringToBool(GetText(), defaultValue);
}

int XmlItem::GetTextAsInt(int defaultValue) const
{
  return RteUtils::StringToInt(GetText(), defaultValue);
}

unsigned XmlItem::GetTextAsUnsigned(unsigned defaultValue) const
{
  return RteUtils::StringToUnsigned(GetText(), defaultValue);
}
unsigned long long XmlItem::GetTextAsULL(unsigned long long defaultValue) const
{
  return RteUtils::StringToULL(GetText(), defaultValue);
}

bool XmlItem::GetItemValueAsBool(const std::string& keyOrTag, bool defaultValue) const
{
  return RteUtils::StringToBool(GetItemValue(keyOrTag), defaultValue);
}

int XmlItem::GetItemValueAsInt(const std::string& keyOrTag, int defaultValue) const
{
  return RteUtils::StringToInt(GetItemValue(keyOrTag), defaultValue);
}

string XmlItem::GetAttributesString(bool quote) const
{
  string s;
  map<string, string>::const_iterator it;
  for (auto [a, v] : m_attributes) {
    if (!s.empty())
      s += " ";
    s += a;
    s += "=";
    if (quote) {
      s += "\"";
    }
    s += v;
    if (quote) {
      s += "\"";
    }
  }
  return s;
}

string XmlItem::GetAttributesAsXmlString() const
{
  return GetAttributesString(true);
}

bool XmlItem::EqualAttributes(const map<string, string>& attributes) const
{
  // all supplied attributes must exist in this ones
  for (auto [a, v] : attributes) {
    auto itm = m_attributes.find(a);
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

bool XmlItem::EqualAttributes(const XmlItem& other) const
{
  if (GetAttributeCount() != other.GetAttributeCount())
    return false;
  return EqualAttributes(other.GetAttributes());
}

bool XmlItem::EqualAttributes(const XmlItem* other) const
{
  if (!other || GetAttributeCount() != other->GetAttributeCount())
    return false;
  return EqualAttributes(other->GetAttributes());
}


bool XmlItem::CompareAttributes(const map<string, string>& attributes) const
{
  // all supplied attributes must exist in this ones
  for (auto [a, v] : attributes) {
    auto itm = m_attributes.find(a);
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

bool XmlItem::Compare(const XmlItem& other) const
{
  if (GetAttributeCount() != other.GetAttributeCount())
    return false;
  return CompareAttributes(other.GetAttributes());
}

bool XmlItem::Compare(const XmlItem* other) const
{
  if (!other || GetAttributeCount() != other->GetAttributeCount())
    return false;
  return CompareAttributes(other->GetAttributes());
}

const std::string XmlItem::GetRootFilePath(bool withTrailingSlash) const
{
  return RteUtils::ExtractFilePath(GetRootFileName(), withTrailingSlash);
}

// End of XmlItem.cpp
