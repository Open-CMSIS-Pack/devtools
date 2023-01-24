/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdDimension.h"
#include "SvdDevice.h"
#include "SvdUtils.h"
#include "XMLTree.h"
#include "ErrLog.h"

using namespace std;

map<SVD_LEVEL, list<string> > SvdDimension::m_allowedTagsDim;


SvdExpression::SvdExpression():
  m_type(SvdTypes::Expression::UNDEF)
{
}

SvdExpression::~SvdExpression()
{
}

bool SvdExpression::CopyItem(SvdExpression *from)
{
  ExprText baseName;
  ExprText description;

  baseName.text     = GetName                 ();
  baseName.pos      = GetNameInsertPos        ();
  description.text  = GetDescription          ();
  description.pos   = GetDescriptionInsertPos ();

  SetType                 (from->GetType                 ());
  SetName                 (from->GetName                 ());
  SetNameInsertPos        (from->GetNameInsertPos        ());
  SetDescription          (from->GetDescription          ());
  SetDescriptionInsertPos (from->GetDescriptionInsertPos ());

  return true;
}

SvdDimension::SvdDimension(SvdItem* parent):
  SvdItem(parent),
  m_dim(SvdItem::VALUE32_NOT_INIT),
  m_dimIncrement(SvdItem::VALUE32_NOT_INIT),
  m_addressBitsUnitsCache(SvdItem::VALUE32_NOT_INIT)
{
  m_dimIndexList.clear();
  SetSvdLevel(L_Dim);
  InitAllowedTags();
}

SvdDimension::~SvdDimension()
{
}

bool SvdDimension::InitAllowedTags()
{
  if(!m_allowedTagsDim.empty()) {
    return true;
  }

  // Peripheral
  m_allowedTagsDim[L_Peripheral].push_back("dim");
  m_allowedTagsDim[L_Peripheral].push_back("dimIncrement");
  m_allowedTagsDim[L_Peripheral].push_back("dimArrayIndex");

  // Cluster
  m_allowedTagsDim[L_Cluster].push_back("dim");
  m_allowedTagsDim[L_Cluster].push_back("dimIncrement");
  m_allowedTagsDim[L_Cluster].push_back("dimIndex");
  m_allowedTagsDim[L_Cluster].push_back("dimName");
  m_allowedTagsDim[L_Cluster].push_back("dimArrayIndex");

  // Register
  m_allowedTagsDim[L_Register].push_back("dim");
  m_allowedTagsDim[L_Register].push_back("dimIncrement");
  m_allowedTagsDim[L_Register].push_back("dimIndex");
  m_allowedTagsDim[L_Register].push_back("dimArrayIndex");

  // Field
  m_allowedTagsDim[L_Field].push_back("dim");
  m_allowedTagsDim[L_Field].push_back("dimIncrement");
  m_allowedTagsDim[L_Field].push_back("dimIndex");
  m_allowedTagsDim[L_Field].push_back("dimName");

  // Interrupt
#if 0   // currently deactivated
  m_allowedTagsDim[L_Interrupt].push_back("dim");
  m_allowedTagsDim[L_Interrupt].push_back("dimIncrement");
  m_allowedTagsDim[L_Interrupt].push_back("dimIndex");
  m_allowedTagsDim[L_Interrupt].push_back("dimName");
#endif

  return true;
}

bool SvdDimension::IsTagAllowed(const string& tag)
{
  const auto parent = GetParent();
  if(!parent) {
    return false;
  }

  const auto svdLevel = parent->GetSvdLevel();
  const auto& allowedTags = m_allowedTagsDim[svdLevel];

  for(const auto& t : allowedTags) {
    if(t == tag) {
      return true;
    }
  }

  return false;
}

bool SvdDimension::Construct(XMLTreeElement* xmlElement)
{
  const auto& tag   = xmlElement->GetTag();
  const auto& value = xmlElement->GetText();
  SetLineNumber(xmlElement->GetLineNumber());

  if(GetTag().empty()) {
    string thisTag { "Dim data: " };
    auto parent = GetParent();
    if(parent) {
      thisTag += parent->GetTag();
    }
    else {
      thisTag += "???";
    }
    SetTag(thisTag);
  }

  if(!IsTagAllowed(tag)) {
    const auto parent = GetParent();
    if(parent) {
      const auto lineNo = xmlElement->GetLineNumber();
      const auto svdLevel = parent->GetSvdLevel();
      LogMsg("M240", TAG(tag), THISLEVEL(), LEVEL2(GetSvdLevelStr(svdLevel)), lineNo);
      parent->Invalidate();
    }

    return true;
  }

  if(!IsModified()) {
    SetModified();

    SetDim(SvdItem::VALUE32_NOT_INIT);
    SetDimIncrement(SvdItem::VALUE32_NOT_INIT);
    SetDimIndex("");
    ClearDimIndexList();
    SetDimName("");
  }

  if(tag == "dim") {
    if(!SvdUtils::ConvertNumber(value, m_dim)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
  }
  else if(tag == "dimIncrement") {
    if(!SvdUtils::ConvertNumber(value, m_dimIncrement)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
  }
  else if(tag == "dimIndex") {
    SetDimIndex(value);
  }
  else if(tag == "dimName") {
    SetDimName(value);
  }

  return true;
}

bool SvdDimension::CalculateDimIndex()
{
  const auto lineNo = GetParent()->GetLineNumber();

  if(m_dim == 1) {
    const auto& name = GetExpression()->GetName();
    LogMsg("M215", NAME(name), lineNo);
  }

  m_dimIndexList.clear();

  // Read the Value
  const auto& value = GetDimIndex();
  if(!value.empty()) {
    string::size_type pos = value.find("-");
    if(pos != string::npos) {   // from - to list
      m_from = value.substr(0, pos);
      SvdUtils::TrimWhitespace(m_from);
      if(pos < value.length()) {
        pos++;
      }
      m_to = value.substr(pos, value.length());
      SvdUtils::TrimWhitespace(m_to);
      CalculateDimIndexFromTo();
    }
    else {
      string index = "";
      for(const auto val : value) {
        if(val == ' ') {
          continue;
        }
        if(val == ',') {
          if(!index.empty()) {
            m_dimIndexList.push_back(index);
          }
          index = "";
        }
        else {
          index += val;
        }
      }
      if(!index.empty()) {
        m_dimIndexList.push_back(index);    // last one
      }
    }
  }


  if(!GetCopiedFrom() && !m_dimIndexList.empty()) {
    const auto exprType = GetExpression()->GetType();
    if(exprType == SvdTypes::Expression::ARRAY) {
      LogMsg("M208", lineNo);
      m_dimIndexList.clear();
    }
    else {
      return true;
    }
  }

  return CalculateDimIndexFromTo();
}

bool SvdDimension::CalculateDimIndexFromTo()
{
  if(!m_dimIndexList.empty()) {
    return true;
  }

  int32_t numFrom=0, numTo=0;
  bool numOk = true;
  if(!m_from.empty()) {
    if(!SvdUtils::ConvertNumber(m_from, numFrom)) {
      numOk = false;
    }
  } else {
    numFrom = 0;
  }

  if(!m_to.empty()) {
    if(!SvdUtils::ConvertNumber(m_to, numTo)) {
      numOk = false;
    }
  } else {
    if(m_dim > 0) {
      numTo = m_dim -1;
    }
  }

  if(!numOk && (m_from.length() != 1 || m_to.length() != 1)) {
    return false;
  }

  if(numOk) {
    int32_t len = numTo - numFrom +1;
    if(len < 0 || (uint32_t)len != m_dim) {
      return false;
    }

    for(int32_t i=numFrom; i<=numTo; i++) {
      auto numStr = SvdUtils::CreateDecNum(i);
      m_dimIndexList.push_back(numStr);
    }
  }
  else {
    // Increment single characters
    char from = m_from.c_str()[0];
    char to   = m_to.c_str()[0];

    int32_t len = to - from +1;
    if(len < 0 || (uint32_t)len != m_dim) {
      return false;
    }

    char res[2] = { 0 };
    for(int32_t i=0; i<len; i++) {
      res[0] = char(from + i);
      m_dimIndexList.push_back(res);
    }
  }

  return true;
}

int32_t SvdDimension::GetAddressBitsUnits()
{
  if(m_addressBitsUnitsCache == SvdItem::VALUE32_NOT_INIT) {
    SvdItem* parent = this;
    while(parent) {
      parent = parent->GetParent();
      const auto device = dynamic_cast<SvdDevice*>(parent);
      if(device) {
        m_addressBitsUnitsCache = device->GetAddressUnitBits();
        break;
      }
    }
    if(m_addressBitsUnitsCache == SvdItem::VALUE32_NOT_INIT) {
      m_addressBitsUnitsCache = 8;        // Cortex-M default
    }
  }

  return m_addressBitsUnitsCache;
}

int32_t SvdDimension::CalcAddressIncrement()
{
  const auto addressBitsUnits  = GetAddressBitsUnits();
  const auto dimIncrement      = GetDimIncrement();

  return (addressBitsUnits * dimIncrement) / 8;
}

bool SvdDimension::CalculateDim()
{
  CalculateNameFromExpression();
  CalculateDisplayNameFromExpression();
  CalculateDescriptionFromExpression();
  CalculateDimIndex();
  CheckItem();

  return true;
}

bool SvdDimension::CalculateNameFromExpression()
{
  const auto item = GetParent();
  if(!item) {
    return true;
  }

  const auto& itemName = item->GetName();
  string name;
  uint32_t insertPos = 0;
  const auto exprType = SvdUtils::ParseExpression(itemName, name, insertPos);

  if(exprType == SvdTypes::Expression::ARRAYINVALID) {
    LogMsg("M241", VALUE(itemName), item->GetLineNumber());
    item->Invalidate();
    return true;
  }
  else if(exprType == SvdTypes::Expression::INVALID) {
    LogMsg("M204", VALUE(itemName), item->GetLineNumber());
    item->Invalidate();
    return true;
  }

  const auto expr = GetExpression();
  if(expr) {
    expr->SetName(name);
    expr->SetType(exprType);
    expr->SetNameInsertPos(insertPos);
  }

  return true;
}

bool SvdDimension::CalculateDisplayNameFromExpression()
{
  const auto item = GetParent();
  if(!item) {
    return true;
  }

  const auto& itemName = item->GetDisplayName();

  string name;
  uint32_t insertPos = 0;
  const auto exprType = SvdUtils::ParseExpression(itemName, name, insertPos);
  if(exprType == SvdTypes::Expression::ARRAYINVALID) {
    LogMsg("M241", VALUE(itemName), item->GetLineNumber());
    item->Invalidate();
    return true;
  }
  else if(exprType == SvdTypes::Expression::INVALID) {
    LogMsg("M204", VALUE(itemName), item->GetLineNumber());
    item->Invalidate();
    return true;
  }

  const auto expr = GetExpression();
  if(expr) {
    expr->SetDisplayName           (name);
    expr->SetDisplayNameInsertPos  (insertPos);
  }

  return true;
}

bool SvdDimension::CalculateDescriptionFromExpression()
{
  const auto item = GetParent();
  if(!item) {
    return true;
  }

  const auto& itemDescr = item->GetDescription();
  string descr;
  uint32_t insertPos = 0;
  const auto exprType = SvdUtils::ParseExpression(itemDescr, descr, insertPos);

  if(exprType == SvdTypes::Expression::ARRAYINVALID) {
    LogMsg("M241", VALUE(itemDescr), item->GetLineNumber());
    item->Invalidate();
    return true;
  }
  else if(exprType == SvdTypes::Expression::INVALID) {
    LogMsg("M204", VALUE(itemDescr), item->GetLineNumber());
    item->Invalidate();
    return true;
  }

  const auto expr = GetExpression();
  if(expr) {
    expr->SetDescription          (descr);
    expr->SetDescriptionInsertPos (insertPos);
  }

  return true;
}

string SvdDimension::CreateName(const string &insert)
{
  const auto expr = GetExpression();
  if(!expr && expr->GetType() == SvdTypes::Expression::UNDEF) {
    return SvdUtils::EMPTY_STRING;
  }

  string name = expr->GetName();
  size_t pos  = expr->GetNameInsertPos();

  if(pos != (size_t)(-1)) {
    if(pos > name.length()) {
      pos = name.length();
    }
    name.insert(pos, insert);
  }

  return name;
}

string SvdDimension::CreateDisplayName(const string &insert)
{
  const auto expr = GetExpression();
  if(!expr && expr->GetType() == SvdTypes::Expression::UNDEF) {
    return SvdUtils::EMPTY_STRING;
  }

  string name = expr->GetDisplayName();
  size_t pos  = expr->GetDisplayNameInsertPos();

  if(!name.empty() && pos != (size_t)(-1)) {
    if(pos > name.length()) pos = name.length();
    name.insert(pos, insert);
  }

  return name;
}

string SvdDimension::CreateDescription(const string &insert)
{
  const auto expr = GetExpression();
  if(!expr && expr->GetType() == SvdTypes::Expression::UNDEF) {
    return SvdUtils::EMPTY_STRING;
  }

  string name = expr->GetDescription();
  size_t pos  = (size_t)expr->GetDescriptionInsertPos();

  if(pos != (size_t)(-1)) {
    if(pos > name.length())
      pos = name.length();
    name.insert(pos, insert);
  }

  return name;
}

bool SvdDimension::CopyItem(SvdItem *from)
{
  const auto pFrom = dynamic_cast<SvdDimension*>(from);
  if(!pFrom) {
    return false;
  }

  const auto  dim          = GetDim         ();
  const auto  dimIncrement = GetDimIncrement();
  const auto& fromIdx      = GetFrom        ();
  const auto& toIdx        = GetTo          ();
  const auto& dimIndex     = GetDimIndex    ();
  const auto& dimName      = GetDimName     ();
  const auto  expr         = GetExpression  ();

  auto exprType = SvdTypes::Expression::UNDEF;
  if(expr) {
    exprType = expr->GetType();
  }

  if(dim          == SvdItem::VALUE32_NOT_INIT)      { SetDim            (pFrom->GetDim            ()); }
  if(dimIncrement == SvdItem::VALUE32_NOT_INIT)      { SetDimIncrement   (pFrom->GetDimIncrement   ()); }
  if(fromIdx      == "")                           { SetFrom           (pFrom->GetFrom           ()); }
  if(toIdx        == "")                           { SetTo             (pFrom->GetTo             ()); }
  if(dimIndex     == "")                           { SetDimIndex       (pFrom->GetDimIndex       ()); }
  if(dimName      == "")                           { SetDimName        (pFrom->GetDimName        ()); }
  if(exprType     ==  SvdTypes::Expression::UNDEF) { expr->CopyItem    (pFrom->GetExpression     ()); }

  SvdItem::CopyItem(from);

  return true;
}

bool SvdDimension::AddToMap(const string& dimIndex)
{
  const auto parent = GetParent();
  if(!parent) {
    return true;
  }

  const auto lineNo = parent->GetLineNumber();
  auto found = m_dimIndexSet.find(dimIndex);
  if(found != m_dimIndexSet.end()) {
    LogMsg("M336", LEVEL("<dimIndex>"), NAME(dimIndex), LINE2(lineNo), lineNo);
  }
  else {
    m_dimIndexSet.insert(dimIndex);
  }

  return true;
}

bool SvdDimension::CheckItem()
{
  if(!IsValid()) {
    return true;
  }

  const auto parent = GetParent();
  if(!parent) {
    return true;
  }

  const auto name = parent->GetNameCalculated();
  const auto lineNo = parent->GetLineNumber();

  const auto& dimIndexList = GetDimIndexList();
  if(!dimIndexList.empty()) {
    const auto num = dimIndexList.size();
    if(m_dim != num) {
      LogMsg("M308", NUM(num), NUM2(m_dim), lineNo);
    }
  }

  // remaining checks are done on SvdItem Level
  const auto dim = GetDim();
  if(dim == SvdItem::VALUE32_NOT_INIT) {
    LogMsg("M213", TAG("dim"), NAME(name), lineNo);
    parent->Invalidate();
  }

  const auto dimInc = GetDimIncrement();
  if(dimInc == SvdItem::VALUE32_NOT_INIT) {
    LogMsg("M213", TAG("dimIncrement"), NAME(name), lineNo);
    parent->Invalidate();
  }

  if(!m_dimIndexSet.empty()) {
    m_dimIndexSet.clear();
  }

  const auto& dimName = GetDimName();
  if(!dimName.empty()) {
    if(dimName.find("%s") != string::npos) {
      LogMsg("M236", TAG("dimName"), NAME(dimName), lineNo);
      parent->Invalidate();
    }
  }

  const auto& n = parent->GetName();
  const auto svdLevel = parent->GetSvdLevel();

  if(svdLevel == L_Cluster && n == "%s" && dimName.empty()) {
    LogMsg("M237", LEVEL("Cluster"), TAG("dimName"), lineNo);
    parent->Invalidate();
  }

  if(!dimIndexList.empty()) {
    for(const auto& dimIndex : dimIndexList) {
      AddToMap(dimIndex);
    }
  }

  const auto expression = GetExpression();
  if(expression) {
    const auto exprType = expression->GetType();
    if(exprType == SvdTypes::Expression::ARRAY || exprType == SvdTypes::Expression::EXTEND) {
#if 0
      const auto dim = GetDim();
      if(dim != SvdItem::VALUE32_NOT_INIT) {
        // TODO
      }
#endif
    }
    else {
      const auto& svdLevelStr = GetSvdLevelStr(parent->GetSvdLevel());
      LogMsg("M239", LEVEL(svdLevelStr), NAME(name), lineNo);
      parent->Invalidate();
    }
  }

  return SvdItem::CheckItem();
}
