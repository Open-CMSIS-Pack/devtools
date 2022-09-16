/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdCluster.h"
#include "SvdRegister.h"
#include "SvdEnum.h"
#include "SvdWriteConstraint.h"
#include "SvdDimension.h"
#include "SvdUtils.h"
#include "XMLTree.h"
#include "ErrLog.h"
#include "SvdDerivedFrom.h"

using namespace std;

SvdCluster::SvdCluster(SvdItem* parent):
  SvdItem(parent),
  m_enumContainer(0),
  m_calcSize(0),
  m_offset(0),
  m_resetValue(0),
  m_resetMask(0),
  m_access(SvdTypes::Access::UNDEF),
  m_modifiedWriteValues(SvdTypes::ModifiedWriteValue::UNDEF),
  m_readAction(SvdTypes::ReadAction::UNDEF)
{
  SetSvdLevel(L_Cluster);
}

SvdCluster::~SvdCluster()
{
}

bool SvdCluster::Construct(XMLTreeElement* xmlElement)
{
  return SvdItem::Construct(xmlElement);
}

bool SvdCluster::ProcessXmlElement(XMLTreeElement* xmlElement)
{
	const auto& tag   = xmlElement->GetTag();
  const auto& value = xmlElement->GetText();

  if(tag == "addressOffset") {
    if(!SvdUtils::ConvertNumber(value, m_offset)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "size") {
    uint32_t num = 0;
    if(!SvdUtils::ConvertNumber(value, num)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    SetBitWidth(num);
    SetModified();
    return true;
  }
  else if(tag == "access") {
    if(!SvdUtils::ConvertAccess(value, m_access, xmlElement->GetLineNumber())) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "resetValue") {
    if(!SvdUtils::ConvertNumber(value, m_resetValue)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "resetMask") {
    if(!SvdUtils::ConvertNumber(value, m_resetMask)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "alternateCluster") {
    //if(!m_alternate.empty())
    //  LogMsg("M203", TAG(tag), VALUE(value), xmlElement->GetLineNumber());
    m_alternate = value;
    return true;
  }
  else if(tag == "headerStructName") {
    ///if(!m_headerStructName.empty())
    //  LogMsg("M203", TAG(tag), VALUE(value), xmlElement->GetLineNumber());
    m_headerStructName = value;
    SetModified();
    return true;
  }
  else if(tag == "register") {
    auto reg = new SvdRegister(this);
    AddItem(reg);
    SetModified();
    return reg->Construct(xmlElement);
	}
  else if(tag == "cluster") {
    auto cluster = new SvdCluster(this);
    AddItem(cluster);
    SetModified();
    return cluster->Construct(xmlElement);
	}
  else if(tag == "dimArrayIndex") {     // enumeratedValues
    if(!m_enumContainer) {
      m_enumContainer = new SvdEnumContainer(this);
    } else {
      LogMsg("M228");   // only one item allowed
      return true;
    }
		return m_enumContainer->Construct(xmlElement);
	}

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdCluster::ProcessXmlAttributes(XMLTreeElement* xmlElement)
{
  return SvdItem::ProcessXmlAttributes(xmlElement);
}

string SvdCluster::GetHeaderTypeName()
{
  if(!m_headerStructName.empty()) {
    return m_headerStructName;
  }

  const auto dim = dynamic_cast<SvdDimension*>(GetParent());
  if(dim) {
    const auto parent = dynamic_cast<SvdCluster*>(dim->GetParent());
    if(parent) {
      const auto& n =  parent->GetHeaderStructName();
      if(!n.empty()) {
        return n;
      }
    }
  }

  return GetHeaderTypeNameCalculated();
}

string SvdCluster::GetHeaderTypeNameHierarchical()
{
  if(!m_headerStructName.empty()) {
    return m_headerStructName;
  }

  if(!IsModified()) {
    const auto copiedFrom = GetCopiedFrom();
    if(copiedFrom) {
      const auto clust = dynamic_cast<SvdCluster*>(copiedFrom);
      if(clust) {
        return clust->GetHeaderTypeNameHierarchical();
      }
    }
  }

  string hierarchicalname;
  const auto parent = GetParent();
  if(parent) {
    hierarchicalname = parent->GetHierarchicalName();
  }

  if(!hierarchicalname.empty()) {
    hierarchicalname += "_";
  }

  const auto name = GetHeaderTypeName();
  hierarchicalname += name;

  return hierarchicalname;
}


string SvdCluster::GetNameCalculated()
{
  string name;

  const auto dim = GetDimension();
  if(dim) {
    const auto& dimName = dim->GetDimName();
    if(!dimName.empty()) {
      name = dimName;
    }
  }

  const auto n = SvdItem::GetNameCalculated();
  if(!n.empty()) {
#if 0
    if(!name.empty())
      name += "_";
#endif
    name += n;
  }

  const auto& altGrp = GetAlternateGroup();
  if(!altGrp.empty()) {
    name += "_";
    name += altGrp;
  }

  return name;
}

bool SvdCluster::CalculateMaxPaddingWidth()
{
  uint32_t maxWidth = 0;

  const auto& childs = GetChildren();
  if(childs.empty()) {
    return false;
  }

  for(const auto child : childs) {
    if(!child->IsValid()) {
      continue;
    }

    const auto width = child->GetEffectiveBitWidth();
    if(width > maxWidth) {
      maxWidth = width;
    }
  }

  if(!maxWidth) {
    maxWidth = 8;
  }

  SetBitWidth(maxWidth);

  return true;
}


bool SvdCluster::Calculate()
{
  const auto& name = GetName();
  if(name.find('%') != string::npos) {
    const auto dim = GetDimension();
    if(!dim) {
      Invalidate();
    }
  }

  const auto headerTypeName = GetHeaderTypeName();
  if(headerTypeName.find('%') != string::npos) {
    const auto dim = GetDimension();
    if(!dim) {
      Invalidate();
    }
  }

  CalculateMaxPaddingWidth();
#if 0
  SvdDimension *dim = GetDimension();
  if(dim) {
    const auto bitWidth = dim->GetDimIncrement();
    bitWidth *= 8;
    if(bitWidth > 32) {
      bitWidth = 32;
    }
    SetBitWidth(8);
  }
#endif

  return SvdItem::Calculate();
}

bool SvdCluster::CalculateDim()
{
  const auto dim = GetDimension();
  if(!dim) {
    return true;
  }

  const auto& childs = dim->GetChildren();
  if(!childs.empty()) {
    dim->ClearChildren();
  }

  dim->CalculateDim();
  const auto& dimIndexList = dim->GetDimIndexList();
  auto offset = GetOffset();
  const auto bitWidth = GetBitWidth();
  string dimIndexText;
  uint32_t dimElementIndex = 0;

  for(const auto& dimIndexname : dimIndexList) {
    const auto newClust = new SvdCluster(dim);
    dim->AddItem(newClust);
    CopyChilds(this, newClust);
    newClust->CopyItem(this);
    newClust->SetName(dim->CreateName(dimIndexname));
    newClust->SetDisplayName(dim->CreateDisplayName(dimIndexname));
    newClust->SetDescription(dim->CreateDescription(dimIndexname));
    //newClust->SetHeaderStructName(headerStructname);
    newClust->SetOffset(offset);
    newClust->SetBitWidth(bitWidth);
    newClust->SetDimElementIndex(dimElementIndex++);
    //newClust->CheckItem();

    offset += dim->GetDimIncrement();

    if(dimElementIndex < 8) {
      if(!dimIndexText.empty()) {
        dimIndexText += ",";
      }

      if(dimElementIndex == 7) {
        dimIndexText += "...";
      }
      else {
        dimIndexText += dimIndexname;
      }
    }
  }

  if(dimIndexList.size() >= 1) {
    auto it = dimIndexList.begin();
    dimIndexText = *it;

    if(dimIndexList.size() > 1) {
      auto rit = dimIndexList.rbegin();
      dimIndexText += "..";
      dimIndexText += *rit;
    }
  }

  string name = dim->CreateName("");
  dim->SetName(name);

  string dName = "[";
  dName += dimIndexText;
  dName += "]";
  string dispName = dim->CreateDisplayName(dName);
  dim->SetDisplayName(dispName);

  string descr = "[";
  descr += dimIndexText;
  descr += "]";
  string description = dim->CreateDescription(descr);
  dim->SetDescription(description);

  return true;
}

bool SvdCluster::CopyItem(SvdItem *from)
{
  const auto pFrom = dynamic_cast<SvdCluster*>(from);
  if(!pFrom) {
    return false;
  }

  const auto& alternate           = GetAlternate            ();
//const auto& headerStructName    = GetHeaderStructName     ();
  const auto  offset              = GetOffset               ();
  const auto  resetValue          = GetResetValue           ();
  const auto  resetMask           = GetResetMask            ();
  const auto  access              = GetAccess               ();
  const auto  modifiedWriteValues = GetModifiedWriteValues  ();
  const auto  readAction          = GetReadAction           ();

  if(alternate           == "")                            { SetAlternate           (pFrom->GetAlternate            ()); }
//if(headerStructName    == "")                            { SetHeaderStructName    (pFrom->GetHeaderStructName     ()); }
  if(offset              == 0 )                            { SetOffset              (pFrom->GetOffset               ()); }
  if(resetValue          == 0 )                            { SetResetValue          (pFrom->GetResetValue           ()); }
  if(resetMask           == 0 )                            { SetResetMask           (pFrom->GetResetMask            ()); }
  if(access              == SvdTypes::Access::UNDEF     )  { SetAccess              (pFrom->GetAccess               ()); }
  if(modifiedWriteValues == SvdTypes::ModifiedWriteValue::UNDEF    )  { SetModifiedWriteValues (pFrom->GetModifiedWriteValues  ()); }
  if(readAction          == SvdTypes::ReadAction::UNDEF )  { SetReadAction          (pFrom->GetReadAction           ()); }

  SvdItem::CopyItem(from);
  CalculateDim();

  return false;
}

uint32_t SvdCluster::GetSize()
{
  if(!m_calcSize) {
    if(!IsModified()) {
      const auto copiedFrom = GetCopiedFrom();
      if(copiedFrom) {
        return copiedFrom->GetSize();
      }

      const auto derivedFrom = GetDerivedFrom();
      if(derivedFrom) {
        const auto item = derivedFrom->GetDerivedFromItem();
        if(item) return item->GetSize();
      }
    }
  }
  return m_calcSize;
}

uint32_t SvdCluster::SetSize(uint32_t size)
{
  m_calcSize = size;

  return true;
}

bool SvdCluster::AddToMap(SvdEnum *enu, map<string, SvdEnum*> &map)
{
  if(!enu) {
    return false;
  }

  const auto name = enu->GetNameCalculated();
  const auto lineNo = enu->GetLineNumber();

  const auto e = map[name];
  if(e) {
    auto nameComplete = enu->GetParent()->GetNameCalculated();
    if(!nameComplete.empty()) {
      nameComplete += ":";
    }
    nameComplete += name;

    LogMsg("M337", LEVEL("Enumerated Value"), NAME(nameComplete), LINE2(e->GetLineNumber()), lineNo);
    enu->Invalidate();
  }
  else map[name] = enu;

  return true;
}

bool SvdCluster::CheckEnumeratedValues()
{
  if(!m_enumContainer || !m_enumContainer->IsValid()) {
    return true;
  }

  const auto dim = GetDimension();
  if(!dim) {
    const string name = m_enumContainer->GetNameCalculated();
    LogMsg("M229", NAME(name), m_enumContainer->GetLineNumber());
    return true;
  }

  const auto exp = dim->GetExpression();
  if(exp && exp->GetType()!= SvdTypes::Expression::ARRAY) {
    const string name = m_enumContainer->GetNameCalculated();
    LogMsg("M229", NAME(name), m_enumContainer->GetLineNumber());
  }

  uint32_t dimElements = dim->GetDim();
  map<string, SvdEnum*> enumMap;

  const auto& childs = m_enumContainer->GetChildren();
  for(const auto child : childs) {
    SvdEnum* enu = dynamic_cast<SvdEnum*>(child);
    if(!enu || !enu->IsValid()) {
      continue;
    }

    const auto& enumName = enu->GetName();
    uint32_t enumValue = (uint32_t) enu->GetValue().u32;

    if(enumValue >= dimElements) {
      const auto regName = GetNameCalculated();
      auto nameComplete = enu->GetParent()->GetNameCalculated();
      if(!nameComplete.empty()) {
        nameComplete += ":";
      }
      nameComplete += enumName;
      LogMsg("M230", NAME(enumName), NUM(enumValue), THISLEVEL(), NAME2(regName), NUM2(dimElements), enu->GetLineNumber());
      enu->Invalidate();
    }

    AddToMap(enu, enumMap);
  }

  return true;
}

bool SvdCluster::CheckItem()
{
  const auto name = GetNameCalculated();
  const auto& n = GetName();
  const auto& headerStructName = GetHeaderStructName();
  auto lineNo = GetLineNumber();

  if(!IsValid()) {
    return true;
  }

  if(n == "%s" && headerStructName.empty()) {
    SetModified();
  }

  if(n != "%s" && name.empty()) {
    return SvdItem::CheckItem();
  }

#if 0
  const auto bitWidth = GetBitWidth();
  if(bitWidth == SvdItem::VALUE32_NOT_INIT) {          // how to handle cluster width for regSorter ?!
    SetBitWidth(8);             // Brute force set to 1 Byte
  }
#endif

  CheckEnumeratedValues();

  const auto& alternate = GetAlternate();
  if(!alternate.empty()) {
    if(!name.compare(alternate)) {
      const auto& svdLevelStr = GetSvdLevelStr(GetSvdLevel());
      LogMsg("M349", LEVEL(svdLevelStr), NAME(alternate), NAME2(name), lineNo);
    }
  }

  const auto childNum = GetChildCount();
  if(!childNum) {
    LogMsg("M328", THISLEVEL(), NAME(name), lineNo);
    Invalidate();
  }
  else if(childNum == 1) {
    LogMsg("M332", THISLEVEL(), NAME(name), lineNo);
  }

#if 0
  // --- Check if there are any valid items
  const list<SvdItem*> &childs = GetChildren();
  uint32_t itemCnt = 0;
  for(const auto item : childs) {
    if(!item || !IsValid()) {
      continue;
    }
    itemCnt++;
  }

  if(!itemCnt) {
    const string& svdLevelStr = GetSvdLevelStr(GetSvdLevel());
    LogMsg("M234", LEVEL(svdLevelStr), NAME(name), lineNo);
    Invalidate();
  }
  // ---
#endif

  const auto dim = GetDimension();
  const auto enumCont = GetEnumContainer();
  if(enumCont) {
    if(dim) {
      const auto expr = dim->GetExpression();
      if(expr) {
        const auto exprType = expr->GetType();
        if(exprType != SvdTypes::Expression::ARRAY) {
          LogMsg("M243", THISLEVEL(), NAME(name), lineNo);    // no array
        }
      }
    }
    else {
      lineNo = enumCont->GetLineNumber();
      LogMsg("M242", THISLEVEL(), NAME(name), lineNo);        // no dim found
    }
  }

  const auto& headerStructname = GetHeaderStructName();
  if(!headerStructname.empty()) {
    if(!headerStructname.compare(name)) {
      LogMsg("M318", THISLEVEL(), TAG("headerStructName"), NAME(name), lineNo);
      SetHeaderStructName("");
      // Invalidate();
    }

    const string hierarchicalName = GetHierarchicalName();
    if(!headerStructname.compare(hierarchicalName)) {
      LogMsg("M371", THISLEVEL(), NAME(hierarchicalName), lineNo);
      SetHeaderStructName("");
      // Invalidate();
    }

    if(headerStructname.find("%") != string::npos) {
      LogMsg("M232", TAG("headerStructName"), NAME(headerStructname), VAL("CHAR", "%"), lineNo);
      SetHeaderStructName("");
      // Invalidate();
    }

#if 0 // why was this check done?
    SvdDimension* dim = GetDimension();
    if(dim && n != "%s") {
      SvdExpression* expr = dim->GetExpression();
      SvdTypes::Expression exprType = expr->GetType();
      if(exprType == SvdTypes::Expression::EXTEND) {
        string nn;
        uint32_t p=0;
        SvdUtils::ParseExpression(name, nn, p);
        LogMsg("M342", LEVEL("Cluster"), NAME(nn), lineNo);
        SetHeaderStructName("");
      }
    }
#endif
  }

#if 0     // calculate max bit width for cluster from all child elements for proper CMSIS Headerfile generation.
  const auto bitWidth = GetEffectiveBitWidth();
  if(bitWidth == SvdItem::VALUE32_NOT_INIT) {
    const std::list<SvdItem*>& childs = GetChildren();
    if(!childs.empty()) {
      for(auto reg : childs) {
        SvdRegister ... // to be done!
      }
    }
  }
#endif

  return SvdItem::CheckItem();
}
