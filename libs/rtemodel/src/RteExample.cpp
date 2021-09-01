/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteExample.cpp
* @brief CMSIS RTE Data model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteExample.h"

#include "RtePackage.h"
#include "RteModel.h"

#include "XMLTree.h"

using namespace std;

RteExample::RteExample(RteItem* parent) :
  RteItem(parent)
{
}


RteExample::~RteExample()
{
  RteExample::Clear();
}


void RteExample::Clear()
{
  m_keywords.clear();
  m_categories.clear();
  for (auto it = m_componentAttributes.begin(); it != m_componentAttributes.end(); it++) {
    delete *it;
  }
  m_componentAttributes.clear();
  RteItem::Clear();
}

bool RteExample::Validate()
{
  m_bValid = RteItem::Validate();
  if (!m_bValid) {
    string msg = CreateErrorString("error", "531", "error(s) in example definition:");
    m_errors.push_front(msg);
  }
  return m_bValid;
}


const string& RteExample::GetVendorString() const
{
  const string& vendor = GetItemValue("vendor");
  if (vendor.empty()) {
    RtePackage* package = GetPackage();
    if (package)
      return package->GetVendorString();
  }
  return vendor;
}

const string& RteExample::GetVersionString() const
{
  const string& ver = GetAttribute("version");
  if (ver.empty()) {
    RtePackage* package = GetPackage();
    if (package)
      return package->GetVersionString();
  }
  return ver;
}

const string& RteExample::GetLoadPath(const string& env) {
  return GetEnvironmentAttribute(env, "load");
}

// Get attribute string of an environment
const string& RteExample::GetEnvironmentAttribute(const string& environment, const string& attribute) {
  if (m_children.empty()) {
    return EMPTY_STRING;
  }

  for (auto it = m_children.begin(); it != m_children.end(); it++) {
    RteItem *pi = *it;
    if (!pi->IsValid()) {
      continue;
    }
    const string& id = pi->GetID();
    if (AlnumCmp::CompareLen(id, environment, false) == 0) {
      return pi->GetAttribute(attribute);
    }
  }

  return EMPTY_STRING;
}

string RteExample::ConstructID()
{
  string id = GetPackageID(false);
  id += "::";
  id += GetName();
  string vendor = DeviceVendor::GetCanonicalVendorName(m_board.GetAttribute("vendor"));
  if (!vendor.empty()) {
    id += ".";
    id += vendor;
  }
  const string& board = m_board.GetAttribute("name");
  if (!board.empty()) {
    id += ".";
    id += board;
  }
  return id;
};



bool RteExample::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  const string& tag = xmlElement->GetTag();
  if (tag == "board") {
    m_board.SetAttributes(xmlElement->GetAttributes());
    return true;
  } else if (tag == "attributes") {
    return ProcessXmlChildren(xmlElement);
  } else if (tag == "keyword") { // child element of "attributes"
    if (!xmlElement->GetText().empty())
      m_keywords.insert(xmlElement->GetText());
    return true;
  } else if (tag == "category") { // child element of "attributes"
    if (!xmlElement->GetText().empty())
      m_categories.insert(xmlElement->GetText());
    return true;
  } else if (tag == "component") { // child element of "attributes"
    RteAttributes* ca = new RteAttributes(xmlElement->GetAttributes());
    m_componentAttributes.push_back(ca);
    return true;
  } else if (tag == "project") {
    return ProcessXmlChildren(xmlElement);
  } else if (tag == "environment") { // child elements of "project"
    RteItem* env = new RteItem(this);
    if (env->Construct(xmlElement)) {
      AddItem(env);
      return true;
    } else {
      delete env;
      return false;
    }
  }

  return RteItem::ProcessXmlElement(xmlElement);
}


bool RteExample::HasKeyword(const string& keyword) const
{
  if (m_board.HasValue(keyword))
    return true;

  // search keywords
  for (auto itm = m_keywords.begin(); itm != m_keywords.end(); itm++) {
    if (WildCards::Match(keyword, *itm)) {
      return true;
    }
  }
  return false;
}

bool RteExample::HasKeywords(const set<string>& keywords) const
{
  for (auto it = keywords.begin(); it != keywords.end(); it++) {
    if (!HasKeyword(*it))
      return false;
  }
  return true;
}

bool RteExample::HasCategory(const string& category) const
{
  return m_categories.find(category) != m_categories.end();
}


bool RteExample::HasCategories(const set<string>& categories) const
{
  for (auto it = categories.begin(); it != categories.end(); it++) {
    if (m_categories.find(*it) == m_categories.end())
      return false;
  }
  return true;
}


/////////////////////////////////////////////////////////////
RteExampleContainer::RteExampleContainer(RteItem* parent) :
  RteItem(parent)
{
}

bool RteExampleContainer::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  const string& tag = xmlElement->GetTag();
  if (tag == "example") {
    RteExample* ex = new RteExample(this);
    if (ex->Construct(xmlElement)) {
      AddItem(ex);
      return true;
    }
    delete ex;
    return false;
  }
  return RteItem::ProcessXmlElement(xmlElement);
}

// End of RteExample.cpp
