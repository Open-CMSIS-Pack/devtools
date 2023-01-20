/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "XmlItem.h"

#include <sstream>
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
    for (auto it = attributes.begin(); it != attributes.end(); it++) {
      if (replaceExisting || !HasAttribute(it->first)) {
        if (AddAttribute(it->first, it->second))
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
  ostringstream ss;
  if (radix == 16) {
    ss << showbase << hex;
  }
  ss << value;
  const string s = ss.str();
  const char* c = s.c_str();
  return SetAttribute(name, c);
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
  return StringToBool(GetAttribute(name), defaultValue);
}

int XmlItem::GetAttributeAsInt(const string& name, int defaultValue) const
{
  return StringToInt(GetAttribute(name), defaultValue);
}

unsigned XmlItem::GetAttributeAsUnsigned(const string& name, unsigned defaultValue) const
{
  return StringToUnsigned(GetAttribute(name), defaultValue);
}

unsigned long long XmlItem::GetAttributeAsULL(const string& name, unsigned long long defaultValue) const
{
  return StringToULL(GetAttribute(name), defaultValue);
}

bool  XmlItem::GetTextAsBool(bool defaultValue) const
{
  return StringToBool(GetText(), defaultValue);
}

int XmlItem::GetTextAsInt(int defaultValue) const
{
  return StringToInt(GetText(), defaultValue);
}

unsigned XmlItem::GetTextAsUnsigned(unsigned defaultValue) const
{
  return StringToUnsigned(GetText(), defaultValue);
}
unsigned long long XmlItem::GetTextAsULL(unsigned long long defaultValue) const
{
  return StringToULL(GetText(), defaultValue);
}

string XmlItem::GetAttributesString(bool quote) const
{
  string s;
  map<string, string>::const_iterator it;
  for (it = m_attributes.begin(); it != m_attributes.end(); it++) {
    if (!s.empty())
      s += " ";
    s += it->first;
    s += "=";
    if (quote) {
      s += "\"";
    }
    s += it->second;
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


bool XmlItem::StringToBool(const std::string& value, bool defaultValue)
{
  if (value.empty())
    return defaultValue;
  return (value == "1" || value == "true");
}


int XmlItem::StringToInt(const std::string& value, int defaultValue)
{
  if (value.empty())
    return defaultValue;
  try {
    return std::stoi(value, 0, 0);
  } catch (const std::exception& ) {
    return defaultValue;
  }
}

unsigned XmlItem::StringToUnsigned(const std::string& value, unsigned defaultValue)
{
  if (value.empty())
    return defaultValue;
  try {
    return std::stoul(value, 0, 0);
  } catch (const std::exception&) {
    return defaultValue;
  }
}

unsigned long long XmlItem::StringToULL(const std::string& value, unsigned long long defaultValue)
{
  if (value.empty()) {
    return (defaultValue);
  }
  int base = 10; // use 10 as default, prevent octal numbers
  if (value.length() > 2) {                                  // 17.1.2014, check for hex number 0xnnn...
    if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {   // 0xnnnn
      base = 16; // use base 16 since it is a hex value
    }
  }
  try {
    return std::stoull(value, 0, base);
  } catch (const std::exception&) {
    return defaultValue;
  }
}

std::string XmlItem::Trim(const std::string &str) {
  static const char *whitespace = " \t\r\n";
  const auto begin = str.find_first_not_of(whitespace);
  if (begin == std::string::npos)
    return EMPTY_STRING;

  const auto end = str.find_last_not_of(whitespace);
  const auto range = end - begin + 1;
  return str.substr(begin, range);
}


// End of XmlItem.cpp
