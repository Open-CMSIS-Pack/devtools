/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdDerivedFrom.h"
#include "SvdDimension.h"
#include "SvdUtils.h"
#include "ErrLog.h"
#include "XMLTree.h"

using namespace std;


SvdDerivedFrom::SvdDerivedFrom(SvdItem* parent):
  SvdItem(parent),
  m_derivedFromItem(nullptr),
  m_bCalculated(false)
{
  SetSvdLevel(L_DerivedFrom);
  m_searchName.clear();
}

SvdDerivedFrom::~SvdDerivedFrom()
{
}

bool SvdDerivedFrom::Construct(XMLTreeElement* xmlElement)
{
  SetLineNumber(xmlElement->GetLineNumber());

  if(GetTag().empty()) {
    string tag { "Derive data: " };
    auto parent = GetParent();
    if(parent) {
      tag += parent->GetTag();
    }
    else {
      tag += "???";
    }

    SetTag(tag);
  }

  ProcessXmlAttributes(xmlElement);

  return true;
}

bool SvdDerivedFrom::ProcessXmlAttributes(XMLTreeElement* xmlElement)
{
  const auto& attributes = xmlElement->GetAttributes();
  if(!attributes.empty()) {
    for(const auto& [tag, value] : attributes) {
      if(tag == "derivedFrom") {
        auto& searchname = GetSearchName();
        SvdUtils::ConvertDerivedNameHirachy(value, searchname);
        SetName(value);
      }
    }
  }

  return true;
}

bool SvdDerivedFrom::CalculateDerivedFrom()
{
  const auto parent = GetParent();
  if(!parent) {
    return false;
  }

  const auto& searchname = GetSearchName();
  const auto svdLevel = parent->GetSvdLevel();

  SvdItem *from=0;
  string lastSearchName;
  if(GetDeriveItem(from, searchname, svdLevel, lastSearchName)) {
    DeriveItem(from);
  } else {
    auto name = GetNameCalculated();
    do {
      auto it = name.find("%");
      if(it == string::npos) {
        break;
      }
      name.erase(it, 1);
    } while(1);

    const auto lineNo = GetLineNumber();
    LogMsg("M206", NAME(name), lineNo);
    Invalidate();
    auto invalidParent = GetParent();
    if(invalidParent) {
      invalidParent->Invalidate();
    }
  }

  return true;
}

bool SvdDerivedFrom::DeriveItem(SvdItem *from)
{
  const auto parent = GetParent();
  if(!parent) {
    return true;
  }

  // If there are already any childs, the item is changed from it's derivedFrom item
  if(parent->GetChildCount()) {
    parent->SetModified();
  }

  // Set derivedFrom source
  SetDerivedFromItem(from);

  // Also copy all element's childs
  CopyChilds(from, parent);

  // Create a copy
  parent->CopyItem(from);

  const auto dim = parent->GetDimension();
  if(dim) {
    dim->SetDimName("");      // dimName cannot be derived on the same level (conflict with %s only)
  }

  return true;
}

bool SvdDerivedFrom::CopyItem(SvdItem *from)
{
  SetCalculated();    // do not derive again, just keep the ref to original item

  SvdItem::CopyItem(from);
  return true;
}
