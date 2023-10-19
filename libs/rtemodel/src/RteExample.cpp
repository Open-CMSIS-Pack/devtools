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
  RteItem(parent),
  m_board(nullptr)
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
  m_componentAttributes.clear();
  m_board = nullptr;
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

const string& RteExample::GetLoadPath(const string& env) const {
  return GetEnvironmentAttribute(env, "load");
}

// Get attribute string of an environment
const string& RteExample::GetEnvironmentAttribute(const string& environment, const string& attribute) const {
  if (m_children.empty()) {
    return EMPTY_STRING;
  }

  for (auto pi : GetChildren()) {
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
  if (m_board) {
    string vendor = DeviceVendor::GetCanonicalVendorName(m_board->GetAttribute("vendor"));
    if (!vendor.empty()) {
      id += ".";
      id += vendor;
    }
    const string& board = m_board->GetAttribute("name");
    if (!board.empty()) {
      id += ".";
      id += board;
    }
  }
  return id;
};


void RteExample::Construct()
{
  RteItem::Construct();
  for (auto item : GetChildren()) {
    const string& text = item->GetText();
    if (!text.empty()) {
      const string& tag = item->GetTag();
      if (tag == "keyword") {
        m_keywords.insert(text);
      } else if (tag == "category") {
        m_categories.insert(text);
      }
    }
  }
}

RteItem* RteExample::CreateItem(const std::string& tag)
{
  if (tag == "project" || tag == "attributes") {
    return this; // processed recursively
  } else if (tag == "board") {
    m_board = new RteItem(this);
    return m_board;
  } else if (tag == "component") { // child element of "attributes"
    RteItem* ca = new RteItem(this);
    m_componentAttributes.push_back(ca);
    return ca;
  }
  return RteItem::CreateItem(tag);
}


bool RteExample::HasKeyword(const string& keyword) const
{
  if (m_board && m_board->HasValue(keyword))
    return true;

  // search keywords
  for (auto kw : m_keywords) {
    if (WildCards::Match(keyword, kw)) {
      return true;
    }
  }
  return false;
}

bool RteExample::HasKeywords(const set<string>& keywords) const
{
  for (auto kw  : keywords) {
    if (!HasKeyword(kw))
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
  for (auto cat : categories) {
    if (m_categories.find(cat) == m_categories.end())
      return false;
  }
  return true;
}


/////////////////////////////////////////////////////////////
RteExampleContainer::RteExampleContainer(RteItem* parent) :
  RteItem(parent)
{
}

RteItem* RteExampleContainer::CreateItem(const std::string& tag)
{
  if(tag == "example") {
    return  new RteExample(this);
  }
  return RteItem::CreateItem(tag);
}

// End of RteExample.cpp
