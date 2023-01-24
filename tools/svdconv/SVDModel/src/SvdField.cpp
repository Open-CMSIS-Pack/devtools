/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdField.h"
#include "SvdEnum.h"
#include "SvdUtils.h"
#include "SvdDimension.h"
#include "ErrLog.h"
#include "SvdWriteConstraint.h"
#include "XMLTree.h"
#include "SvdRegister.h"

using namespace std;


SvdFieldContainer::SvdFieldContainer(SvdItem* parent):
  SvdItem(parent)
{
  SetSvdLevel(L_Fields);
}

SvdFieldContainer::~SvdFieldContainer()
{
}

bool SvdFieldContainer::Construct(XMLTreeElement* xmlElement)
{
  return SvdItem::Construct(xmlElement);
}

bool SvdFieldContainer::ProcessXmlElement(XMLTreeElement* xmlElement)
{
	const auto& tag = xmlElement->GetTag();

  if(tag == "field") {
    const auto field = new SvdField(this);
    AddItem(field);
    return field->Construct(xmlElement);
	}

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdFieldContainer::CopyItem(SvdItem *from)
{
  SvdItem::CopyItem(from);

  return false;
}



SvdField::SvdField(SvdItem* parent):
  SvdItem(parent),
  m_writeConstraint(nullptr),
  m_lsb(SvdItem::VALUE32_NOT_INIT),
  m_msb(SvdItem::VALUE32_NOT_INIT),
  m_offset(SvdItem::VALUE64_NOT_INIT),
  m_access(SvdTypes::Access::UNDEF),
  m_modifiedWriteValues(SvdTypes::ModifiedWriteValue::UNDEF),
  m_readAction(SvdTypes::ReadAction::UNDEF)
{
  SetSvdLevel(L_Field);
}

SvdField::~SvdField()
{
  delete m_writeConstraint;
}

bool SvdField::Construct(XMLTreeElement* xmlElement)
{
  return SvdItem::Construct(xmlElement);
}

bool SvdField::ProcessXmlElement(XMLTreeElement* xmlElement)
{
	const auto& tag   = xmlElement->GetTag();
  const auto& value = xmlElement->GetText();

  if(tag == "bitOffset") {
    if(!SvdUtils::ConvertNumber(value, m_offset)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "bitWidth") {
    uint32_t num = 0;
    if(!SvdUtils::ConvertNumber(value, num)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    SetBitWidth(num);
    return true;
  }
  else if(tag == "lsb") {
    if(!SvdUtils::ConvertNumber(value, m_lsb)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    SetOffset(SvdItem::VALUE64_NOT_INIT);    // reset derived values
    SetBitWidth(SvdItem::VALUE32_NOT_INIT);
    return true;
  }
  else if(tag == "msb") {
    if(!SvdUtils::ConvertNumber(value, m_msb)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    SetOffset(SvdItem::VALUE64_NOT_INIT);    // reset derived values
    SetBitWidth(SvdItem::VALUE32_NOT_INIT);
    return true;
  }
  else if(tag == "bitRange") {
    if(!SvdUtils::ConvertBitRange(value, m_msb, m_lsb)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    SetOffset(SvdItem::VALUE64_NOT_INIT);    // reset derived values
    SetBitWidth(SvdItem::VALUE32_NOT_INIT);
    return true;
  }
  else if(tag == "access") {
    if(!SvdUtils::ConvertAccess(value, m_access, xmlElement->GetLineNumber())) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "modifiedWriteValues") {
    if(!SvdUtils::ConvertModifiedWriteValues(value, m_modifiedWriteValues, xmlElement->GetLineNumber())) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "readAction") {
    if(!SvdUtils::ConvertReadAction(value, m_readAction, xmlElement->GetLineNumber())){
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "writeConstraint") {
    if(!m_writeConstraint) {
      m_writeConstraint = new SvdWriteConstraint(this);
    }
    return m_writeConstraint->Construct(xmlElement);
  }
  else if(tag == "enumeratedValues") {
    SvdEnumContainer *enumContainer = new SvdEnumContainer(this);
    AddItem(enumContainer);
		return enumContainer->Construct(xmlElement);
	}

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdField::ProcessXmlAttributes(XMLTreeElement* xmlElement) {
  return SvdItem::ProcessXmlAttributes(xmlElement);
}

bool SvdField::Calculate()
{
  bool ok = SvdItem::Calculate();

  if(!GetDimension()) {
    auto name = GetName();
    string::size_type pos = name.find('%');
    if(pos != string::npos) {
      name.erase(pos, 1);
      SetName(name);
      Invalidate();
    }
  }

  if(m_offset == SvdItem::VALUE64_NOT_INIT && GetBitWidth() == (int32_t)SvdItem::VALUE32_NOT_INIT) {
    if(m_lsb != SvdItem::VALUE32_NOT_INIT && m_msb != SvdItem::VALUE32_NOT_INIT) { // && m_msb >= m_lsb) {
      m_offset = m_lsb;
      SetBitWidth(m_msb - m_lsb +1);
    }
  }

  return ok;
}

bool SvdField::CalculateDim()
{
  const auto dim = GetDimension();
  if(!dim) {
    return true;
  }

  Calculate();

  const auto& childs = dim->GetChildren();
  if(!childs.empty()) {
    dim->ClearChildren();
  }

  dim->CalculateDim();

  const auto dimExpression = dim->GetExpression();
  SvdTypes::Expression exprType = SvdTypes::Expression::NONE;
  if(dimExpression) {
    exprType = dimExpression->GetType();
  }

  if(exprType == SvdTypes::Expression::ARRAY) {
    const auto lineNo = GetLineNumber();
    const auto& svdLevelStr = GetSvdLevelStr(GetSvdLevel());
    const auto name = GetNameCalculated();
    LogMsg("M235", LEVEL(svdLevelStr), NAME(name), lineNo);
    Invalidate();
    return true;
  }

  const auto& dimIndexList = dim->GetDimIndexList();
  auto offset = GetOffset();
  uint32_t dimElementIndex = 0;
  string dimIndexText;

  for(const auto& index : dimIndexList) {
    const auto newField = new SvdField(dim);
    dim->AddItem(newField);
    CopyChilds(this, newField);
    newField->CopyItem            (this);
    newField->SetName             (dim->CreateName(index));
    newField->SetDisplayName      (dim->CreateDisplayName(index));
    newField->SetDescription      (dim->CreateDescription(index));
    newField->SetOffset           (offset);
    newField->SetDimElementIndex  (dimElementIndex++);
    newField->CheckItem();
    offset += dim->CalcAddressIncrement(); //GetDimIncrement();

    if(dimElementIndex < 8) {
      if(!dimIndexText.empty()) dimIndexText += ",";
      if(dimElementIndex == 7) {
        dimIndexText += "...";
      } else {
        dimIndexText += index;
      }
    }
  }

  if(dimIndexText.empty()) {
    if(dimIndexList.size() >= 1) {
      dimIndexText = *dimIndexList.begin();

      if(dimIndexList.size() > 1) {
        const auto& dimIndexTextEnd = *dimIndexList.rbegin();
        dimIndexText += "..";
        dimIndexText += dimIndexTextEnd;
      }
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

bool SvdField::CopyItem(SvdItem *from)
{
  const auto pFrom = dynamic_cast<SvdField*>(from);
  if(!pFrom) {
    return false;
  }

  const auto offset              = GetOffset             ();
  const auto lsb                 = GetLsb                ();
  const auto msb                 = GetMsb                ();
  const auto access              = GetAccess             ();
  const auto modifiedWriteValues = GetModifiedWriteValue ();
  const auto readAction          = GetReadAction         ();

  if(offset              == SvdItem::VALUE64_NOT_INIT    ) { SetOffset             (pFrom->GetOffset               ()); }
  if(lsb                 == SvdItem::VALUE32_NOT_INIT    ) { SetLsb                (pFrom->GetLsb                  ()); }
  if(msb                 == SvdItem::VALUE32_NOT_INIT    ) { SetMsb                (pFrom->GetMsb                  ()); }
  if(access              == SvdTypes::Access::UNDEF    ) { SetAccess             (pFrom->GetAccess               ()); }
  if(modifiedWriteValues == SvdTypes::ModifiedWriteValue::UNDEF   ) { SetModifiedWriteValue (pFrom->GetModifiedWriteValue   ()); }
  if(readAction          == SvdTypes::ReadAction::UNDEF) { SetReadAction         (pFrom->GetReadAction           ()); }

  SvdItem::CopyItem(from);
  CalculateDim();

  return true;
}

bool SvdField::GetValuesDescriptionString(string &longDescr)
{
  uint32_t bitWidth = GetBitWidth();
  if(bitWidth > MAX_BITWIDTH_FOR_COMBO) {
    return true;
  }

  uint32_t bitMaxNum = (uint32_t)(((uint64_t)1 << (uint64_t)bitWidth));
  const auto& conts = GetEnumContainer();
  if(conts.empty()) {
    return true;
  }

  const auto cont = *conts.begin();
  const auto& childs = cont->GetChildren();
  if(childs.empty()) {
    return true;
  }

  map<uint32_t, SvdEnum*> enumValues;

  for(const auto child : childs) {
    const auto enu = dynamic_cast<SvdEnum*>(child);
    if(!enu || !enu->IsValid()) {
      continue;
    }

    const auto val = enu->GetValue().u32;
    enumValues[val] = enu;
  }

  if(enumValues.empty()) {
    return true;
  }

  for(uint32_t i=0; i<bitMaxNum; i++) {
    auto element = enumValues.find(i);
    if(element != enumValues.end()) {
      const auto enu = element->second;
      if(enu) {
        const auto name  = enu->GetNameCalculated();
        const auto descr = enu->GetDescriptionCalculated();
        const auto val   = (uint32_t)enu->GetValue().u32;

        if(!longDescr.empty()) {
          longDescr += "\\n";
        }
        longDescr += SvdUtils::CreateDecNum(val);
        longDescr += " : ";

        if(!name.empty()) {
          longDescr += name;
        }
        if(!descr.empty()) {
          if(!name.empty()) {
            longDescr += " = ";
          }
          longDescr += descr;
        }
      }
      continue;
    }

    if(!longDescr.empty()) {
      longDescr += "\\n";
    }
    longDescr += SvdUtils::CreateDecNum(i);
    longDescr += " : ";
    //longDescr += " = ";
    longDescr += "Reserved - do not use";
  }

  return true;
}

bool SvdField::AddToMap(SvdEnum *enu, map<uint32_t, SvdEnum*> &map)
{
  const auto name      = enu->GetNameCalculated();
  const auto lineNo           = enu->GetLineNumber();
  const auto enumValue = (uint32_t) enu->GetValue().u32;

  const auto e = map[enumValue];
  if(e) {
    const auto& enumUsageStr = SvdTypes::GetEnumUsage(enu->GetEffectiveEnumUsage());
    const auto& eUsageStr    = SvdTypes::GetEnumUsage(e->GetEffectiveEnumUsage());
    const auto& n            = e->GetName();
    const auto  lNo          = e->GetLineNumber();

    auto nameComplete = enu->GetParent()->GetNameCalculated();
    if(!nameComplete.empty()) {
      nameComplete += ":";
    }
    nameComplete += name;

    auto nComplete  = enu->GetParent()->GetNameCalculated();
    if(!nComplete.empty()) {
      nComplete += ":";
    }
    nComplete += n;

    if(enumValue < 64) {
      LogMsg("M333", NUM(enumValue), NAME(nameComplete), USAGE(enumUsageStr), NAME2(nComplete), USAGE2(eUsageStr), LINE2(lNo), lineNo);
    } else {
      string text = SvdUtils::CreateHexNum(enumValue, 8);
      LogMsg("M333", NUMTXT(text), NAME(nameComplete), USAGE(enumUsageStr), NAME2(nComplete), USAGE2(eUsageStr), LINE2(lNo), lineNo);
    }
    enu->Invalidate();

    return false;
  }
  else {
    map[enumValue] = enu;
  }

  return true;
}

bool SvdField::AddToMap(SvdEnum *enu, map<string, SvdEnum*> &map)
{
  const auto name = enu->GetNameCalculated();
  const auto lineNo = enu->GetLineNumber();

  const auto e = map[name];
  if(e) {
    LogMsg("M337", LEVEL("Enumerated Value"), NAME(name), LINE2(e->GetLineNumber()), lineNo);    // M545 as WARNING
    enu->Invalidate();
  }
  else {
    map[name] = enu;
  }

  return true;
}

bool SvdField::AddToMap(SvdEnumContainer *enuCont, map<string, SvdEnumContainer*> &map)
{
  const auto name = enuCont->GetNameCalculated();
  const auto lineNo = enuCont->GetLineNumber();

  const auto e = map[name];
  if(e) {
    LogMsg("M336", LEVEL("Enumerated Values Container"), NAME(name), LINE2(e->GetLineNumber()), lineNo);
    enuCont->Invalidate();
  }
  else {
    map[name] = enuCont;
  }

  return true;
}

/*** Field test
 * EnumContainers must be unique in field
 * EnumValues must be unique in enumContainer
 ***/
bool SvdField::CheckItem()
{
  if(!IsValid()) {
    return true;
  }

  const auto name = GetNameCalculated();
  auto lineNo = GetLineNumber();

  const auto offs  = GetOffset();
  const auto width = GetBitWidth();

  const auto& n = GetName();
  if(n != "%s") {
    if(name.empty()) {
      return SvdItem::CheckItem();
    }
  }

  if(offs == SvdItem::VALUE64_NOT_INIT || width == (int32_t)SvdItem::VALUE32_NOT_INIT) {
    LogMsg("M311", NAME(name), lineNo);
    SetOffset(0);
    SetBitWidth(1);
    Invalidate();
    return true;
  }

  if(offs > FIELD_MAX_OFFSET) {
    LogMsg("M309", NAME(name), NUM(offs), lineNo);
    SetOffset(FIELD_MAX_OFFSET);
    Invalidate();
  }
  if(width > FIELD_MAX_BITWIDTH) {
    LogMsg("M310", NAME(name), NUM(width), lineNo);
    SetBitWidth(FIELD_MAX_BITWIDTH);
    Invalidate();
  }
  if(width < 0) {
    LogMsg("M313", NAME(name), NUM(width), lineNo);
    SetBitWidth(1);
    Invalidate();
  }

  auto parent = GetParent();
  if(parent) {
    const auto dim = dynamic_cast<SvdDimension*>(parent);
    if(dim) {
      parent = parent->GetParent();
      if(parent) {
        parent = parent->GetParent();
      }
    }
    const auto reg = dynamic_cast<SvdRegister*>(parent->GetParent());
    if(reg) {
      const auto regWidth = reg->GetEffectiveBitWidth();
      const auto l = reg->GetLineNumber();
      if(width > (int64_t)regWidth || offs+width > regWidth) {
        LogMsg("M324", NAME(name), BITRANGE(offs+width-1, offs, true), NAME2(reg->GetName()), NUM(regWidth), LINE2(l), lineNo);
        Invalidate();
      }
    }

    const auto regAccess    = reg->GetEffectiveAccess();
    const auto fieldAccess  = GetEffectiveAccess();

    if(!SvdUtils::IsMatchAccess(fieldAccess, regAccess)) {
      LogMsg("M367", lineNo);
    }
  }

  map<string, SvdEnumContainer*> enumContMap;
  uint64_t fieldMaxVal = (1ULL << (uint64_t)width) -1;

  const auto& enumConts = GetEnumContainer();
  if(enumConts.empty() && width < 6) {        // message "please add enum values"
    LogMsg("M347", NAME(name), lineNo);
  }

  uint32_t cnt = 0;
  map<SvdTypes::EnumUsage, SvdEnumContainer*> enumContainerRW;

  for(const auto enumCont : enumConts) {
    SvdEnumContainer* cont = dynamic_cast<SvdEnumContainer*>(enumCont);
    if(!cont) {
      continue;
    }

    auto enumContainerName = cont->GetNameCalculated();
    const auto lNo = cont->GetLineNumber();
    if(enumContainerName.empty()) {
      enumContainerName = "not named";
    }

    cnt++;
    if(cnt > 2) {
      LogMsg("M375", THISLEVEL(), NAME(name), NAME2(enumContainerName), lNo);
      LogMsg("M374", lNo);
      cont->Invalidate();
    }

    if(!cont->IsValid()) {
      continue;
    }

    const auto usage = cont->GetEffectiveEnumUsage();
    const auto eCont = enumContainerRW[usage];
    if(eCont) {
      LogMsg("M376", THISLEVEL(), NAME(name), NAME2(enumContainerName), USAGE(SvdTypes::GetEnumUsage(usage)), LINE2(eCont->GetLineNumber()), lNo);
      LogMsg("M374", lNo);
      cont->Invalidate();
    }
    else {
      enumContainerRW[usage] = cont;
    }
  }

  const auto eCont = enumContainerRW[SvdTypes::EnumUsage::READWRITE];
  if(eCont) {
    auto eC = enumContainerRW[SvdTypes::EnumUsage::READ];
    if(eC) {
      const auto lNo = eC->GetLineNumber();
      LogMsg("M377", THISLEVEL(), NAME(name), NAME2(eC->GetNameCalculated()), USAGE(SvdTypes::GetEnumUsage(eC->GetUsage())), NAME3(eCont->GetNameCalculated()), LINE2(eCont->GetLineNumber()), lNo);
      LogMsg("M374", lNo);
      eC->Invalidate();
    }

    eC = enumContainerRW[SvdTypes::EnumUsage::WRITE];
    if(eC) {
      const auto lNo = eC->GetLineNumber();
      LogMsg("M377", THISLEVEL(), NAME(name), NAME2(eC->GetNameCalculated()), USAGE(SvdTypes::GetEnumUsage(eC->GetUsage())), NAME3(eCont->GetNameCalculated()), LINE2(eCont->GetLineNumber()), lNo);
      LogMsg("M374", lNo);
      eC->Invalidate();
    }
  }

  for(const auto enumCont : enumConts) {
    SvdEnumContainer* cont = dynamic_cast<SvdEnumContainer*>(enumCont);
    if(!cont || !cont->IsValid()) {
      continue;
    }

    auto enumContainerName = cont->GetNameCalculated();
    //AddToMap(cont, enumContMap);       // 14.03.2016 Enum Container for "read" and "write" can have the same name, because they are suffixed with "_R" or "_W"

    if(!cont->IsValid()) {
      continue;
    }

    map<string, SvdEnum*> enumMap;
    map<uint32_t, SvdEnum*> enumValMap;

    const auto& childs = cont->GetChildren();
    for(const auto child : childs) {
      const auto enu = dynamic_cast<SvdEnum*>(child);
      if(!enu || !enu->IsValid() || enu->IsDefault()) {
        continue;
      }

      const auto& enumName  = enu->GetName();
      const auto enumValue = (uint32_t) enu->GetValue().u32;
      lineNo = enu->GetLineNumber();

      if(enumName.empty()) {
        continue;
      }

      AddToMap(enu, enumMap);

      if(enumValue > fieldMaxVal) {
        LogMsg("M335", BITRANGE(offs+width-1, offs, true), NAME(enumName), NUM(enumValue), NAME2(name), lineNo);
        enu->Invalidate();
      }

      AddToMap(enu, enumValMap);
    }
  }

  return SvdItem::CheckItem();
}
