/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdRegister.h"
#include "SvdCluster.h"
#include "SvdField.h"
#include "SvdEnum.h"
#include "SvdWriteConstraint.h"
#include "SvdDimension.h"
#include "SvdDerivedFrom.h"
#include "SvdUtils.h"
#include "XMLTree.h"
#include "ErrLog.h"

using namespace std;


SvdRegisterContainer::SvdRegisterContainer(SvdItem* parent):
  SvdItem(parent)
{
  SetSvdLevel(L_Registers);
}

SvdRegisterContainer::~SvdRegisterContainer()
{
}

bool SvdRegisterContainer::Construct(XMLTreeElement* xmlElement)
{
  return SvdItem::Construct(xmlElement);
}

bool SvdRegisterContainer::ProcessXmlElement(XMLTreeElement* xmlElement)
{
	const auto& tag = xmlElement->GetTag();

  if(tag == "register") {
    const auto reg = new SvdRegister(this);
    AddItem(reg);
    return reg->Construct(xmlElement);
	}
  else if(tag == "cluster") {
    const auto cluster = new SvdCluster(this);
    AddItem(cluster);
    return cluster->Construct(xmlElement);
	}

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdRegisterContainer::CopyItem(SvdItem *from)
{
  SvdItem::CopyItem(from);

  return false;
}


SvdRegister::SvdRegister(SvdItem* parent):
  SvdItem(parent),
  m_svdWriteConstraint(nullptr),
  m_enumContainer(nullptr),
  m_hasValidFields(true),
  m_accessMaskValid(false),
  m_offset(SvdItem::VALUE64_NOT_INIT),
  m_resetValue(0),
  m_resetMask(0),
  m_accessMaskRead(0xffffffff),
  m_accessMaskWrite(0xffffffff),
  m_access(SvdTypes::Access::UNDEF),
  m_modifiedWriteValues(SvdTypes::ModifiedWriteValue::UNDEF),
  m_readAction(SvdTypes::ReadAction::UNDEF)
{
  SetSvdLevel(L_Register);
}

SvdRegister::~SvdRegister()
{
}


bool SvdRegister::Construct(XMLTreeElement* xmlElement)
{
  bool success = SvdItem::Construct(xmlElement);

  const auto bitWidth = GetBitWidth();
  if(bitWidth == (int32_t)SvdItem::VALUE32_NOT_INIT) {       // default, derive from above levels
    return true;
  }

  if(bitWidth != 8 && bitWidth != 16 && bitWidth != 24 && bitWidth != 32 && bitWidth != 64) {
    LogMsg("M302", NAME(GetNameCalculated()), NUM(bitWidth), GetLineNumber());
    Invalidate();
  }

  return success;
}

bool SvdRegister::ProcessXmlElement(XMLTreeElement* xmlElement)
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
  else if(tag == "modifiedWriteValues") {
    if(!SvdUtils::ConvertModifiedWriteValues(value, m_modifiedWriteValues, xmlElement->GetLineNumber())) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "readAction") {
    if(!SvdUtils::ConvertReadAction(value, m_readAction, xmlElement->GetLineNumber())) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "alternateRegister") {
    m_alternate = value;
    return true;
  }
  else if(tag == "alternateGroup") {
    m_alternateGroup = value;
    return true;
  }
  else if(tag == "dataType") {
    if(!SvdUtils::ConvertDataType(value, m_dataType, xmlElement->GetLineNumber()))
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    SetModified();
    return true;
  }
  else if(tag == "writeConstraint") {
    if(!m_svdWriteConstraint) {
      m_svdWriteConstraint = new SvdWriteConstraint(this);
    }
    return m_svdWriteConstraint->Construct(xmlElement);
  }
  else if(tag == "fields") {
    auto fieldContainer = GetFieldContainer();
    if(!fieldContainer) {
      fieldContainer = new SvdFieldContainer(this);
      AddItem(fieldContainer);
    }
    SetModified();
		return fieldContainer->Construct(xmlElement);
	}
  else if(tag == "dimArrayIndex") {
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

bool SvdRegister::ProcessXmlAttributes(XMLTreeElement* xmlElement)
{
  return SvdItem::ProcessXmlAttributes(xmlElement);
}

SvdFieldContainer* SvdRegister::GetFieldContainer() const
{
  if(!GetChildCount()) {
    return nullptr;
  }

  const auto cont = dynamic_cast<SvdFieldContainer*>(*(GetChildren().begin()));

  return cont;
}

string SvdRegister::GetNameCalculated()
{
  auto name = SvdItem::GetNameCalculated();
  const auto& altGrp = GetAlternateGroup();
  if(!altGrp.empty()) {
    name += "_";
    name += altGrp;
  }

  return name;
}

bool SvdRegister::Calculate()
{
  return SvdItem::Calculate();
}

bool SvdRegister::CalculateDim()
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
  uint32_t dimElementIndex = 0;
  string dimIndexText;

  for(const auto& dimIndex : dimIndexList) {
    const auto newReg = new SvdRegister(dim);
    dim->AddItem(newReg);
    CopyChilds(this, newReg);

    newReg->CopyItem            (this);
    newReg->SetName             (dim->CreateName(dimIndex));
    newReg->SetDisplayName      (dim->CreateDisplayName(dimIndex));
    newReg->SetDescription      (dim->CreateDescription(dimIndex));
    newReg->SetOffset           (offset);
    newReg->SetDimElementIndex  (dimElementIndex++);
    newReg->CheckItem           ();
    offset += dim->CalcAddressIncrement(); //GetDimIncrement();

    if(dimElementIndex < 8) {
      if(!dimIndexText.empty()) {
        dimIndexText += ",";
      }

      if(dimElementIndex == 7) {
        dimIndexText += "...";
      } else {
        dimIndexText += dimIndex;
      }
    }
  }

  if(dimIndexList.size() >= 1) {
    dimIndexText = *dimIndexList.begin();

    if(dimIndexList.size() > 1) {
      dimIndexText += "..";
      dimIndexText += *dimIndexList.rbegin();
    }
  }

  auto name = dim->CreateName("");
  dim->SetName(name);

  string dName = "[";
  dName += dimIndexText;
  dName += "]";
  auto dispName = dim->CreateDisplayName(dName);
  dim->SetDisplayName(dispName);

  string descr = "[";
  descr += dimIndexText;
  descr += "]";
  auto description = dim->CreateDescription(descr);
  dim->SetDescription(description);

  return true;
}

bool SvdRegister::CopyItem(SvdItem *from)
{
  const auto pFrom = dynamic_cast<SvdRegister*>(from);
  if(!pFrom) {
    return false;
  }

  const auto& alternate           = GetAlternate          ();
  const auto& alternateGroup      = GetAlternateGroup     ();
  const auto& dataType            = GetDataType           ();
  const auto  offset              = GetOffset             ();
  const auto  width               = GetBitWidth           ();
  const auto  resetValue          = GetResetValue         ();
  const auto  resetMask           = GetResetMask          ();
  const auto  access              = GetAccess             ();
  const auto  modifiedWriteValues = GetModifiedWriteValue ();
  const auto  readAction          = GetReadAction         ();

  if(alternate           == "")                                   { SetAlternate           (pFrom->GetAlternate          ()); }
  if(alternateGroup      == "")                                   { SetAlternateGroup      (pFrom->GetAlternateGroup     ()); }
  if(dataType            == "")                                   { SetDataType            (pFrom->GetDataType           ()); }
  if(offset              == SvdItem::VALUE64_NOT_INIT)            { SetOffset              (pFrom->GetOffset             ()); }
  if(width               == 0 )                                   { SetBitWidth            (pFrom->GetBitWidth           ()); }
  if(resetValue          == 0 )                                   { SetResetValue          (pFrom->GetResetValue         ()); }
  if(resetMask           == 0 )                                   { SetResetMask           (pFrom->GetResetMask          ()); }
  if(access              == SvdTypes::Access::UNDEF)              { SetAccess              (pFrom->GetAccess             ()); }
  if(modifiedWriteValues == SvdTypes::ModifiedWriteValue::UNDEF)  { SetModifiedWriteValues (pFrom->GetModifiedWriteValue ()); }
  if(readAction          == SvdTypes::ReadAction::UNDEF)          { SetReadAction          (pFrom->GetReadAction         ()); }

  SvdItem::CopyItem(from);
  CalculateDim();

  return false;
}

string SvdRegister::GetHeaderTypeName()
{
  const auto& dataType2 = GetDataType();
  if(!dataType2.empty()) {
    return dataType2;
  }

  const auto regWidth = GetEffectiveBitWidth() / 8;
  const auto& dataType = SvdUtils::GetDataTypeString(regWidth);

  return dataType;
}

// calculates and returns the name used in CMSIS Headerfile
string SvdRegister::GetHeaderFileName()
{
  const auto regName  = GetNameCalculated();
  const auto& prepend = GetPrependToName();
  const auto& append  = GetAppendToName();

  string name;
  if(!prepend.empty()) {
    name += prepend;
  }
  name += regName;
  if(!append.empty()) {
    name += append;
  }

  return name;
}

/*** Calculate accessType from fields
 *
 * - GetEffectiveAccess(): calculates accessType through the SVD defined value
 * - calculate through all fields (also undefined fields) and calculate the "highest" accessType
 */
SvdTypes::Access SvdRegister::GetAccessCalculated()
{
  const auto fields = GetFieldContainer();
  if(!fields) {
    return GetEffectiveAccess();
  }

  const auto& childs = fields->GetChildren();
  if(childs.empty()) {
    return GetEffectiveAccess();
  }

  const auto regWidth = GetEffectiveBitWidth();
  const auto mask = ((uint64_t)1 << regWidth) -1;
  auto bits = (uint64_t)0U;
  auto access = SvdTypes::Access::UNDEF; //GetEffectiveAccess();

  for(const auto child : childs) {
    const auto field = dynamic_cast<SvdField*>(child);
    if(!field) {
      continue;
    }

    const auto fieldWidth = field->GetEffectiveBitWidth();
    bits |= ((uint64_t)1 << fieldWidth) -1;

    SvdTypes::Access acc = field->GetEffectiveAccess();
    access = SvdUtils::CalcAccessResult(acc, access);
  }

  if(bits != mask) {   // remaining undefined bitfields
    const auto acc = GetEffectiveAccess();
    access = SvdUtils::CalcAccessResult(acc, access);
  }

  if(access == SvdTypes::Access::UNDEF) {
    access = GetEffectiveAccess();
  }

  return access;
}

bool SvdRegister::CalcAccessMask()
{
  const auto fields = GetFieldContainer();
  if(!fields) {
    return true;
  }

  const auto& childs = fields->GetChildren();
  if(childs.empty())  {
    return true;
  }

  const auto regWidth = GetEffectiveBitWidth();
  m_accessMaskRead  = (((uint32_t)((uint64_t)1 << regWidth)-1) << 0);
  m_accessMaskWrite = 0;

  for(const auto child : childs) {
    SvdField* field = dynamic_cast<SvdField*>(child);
    if(!field) {
      continue;
    }

    const auto bitWidth = field->GetEffectiveBitWidth();
    const auto firstBit = field->GetOffset();
    auto accessMask = 0;
    accessMask  = (((uint32_t)((uint64_t)1 << bitWidth)-1) << firstBit);
    accessMask &=  ((uint32_t)((uint64_t)1 << regWidth)-1);

    const auto accType = field->GetEffectiveAccess();
    switch(accType) {
      case SvdTypes::Access::READONLY:
      //m_accessMaskRead  |=  accessMask;
        m_accessMaskWrite &= ~accessMask;
        break;
      case SvdTypes::Access::WRITEONLY:
      //m_accessMaskRead  &= ~accessMask;
        m_accessMaskWrite |=  accessMask;
        break;
      case SvdTypes::Access::READWRITE:
      case SvdTypes::Access::WRITEONCE:
      case SvdTypes::Access::READWRITEONCE:
      //m_accessMaskRead  |=  accessMask;
        m_accessMaskWrite |=  accessMask;
        break;
      case SvdTypes::Access::UNDEF:
      default:
        break;
    }
  }

  m_accessMaskValid = true;

  return true;
}

uint64_t SvdRegister::GetAccessMaskRead()
{
  if(!m_accessMaskValid) {
    CalcAccessMask();
  }

  return m_accessMaskRead;
}

uint64_t SvdRegister::GetAccessMaskWrite()
{
  if(!m_accessMaskValid) {
    CalcAccessMask();
  }

  return m_accessMaskWrite;
}

uint64_t SvdRegister::GetAccessMask()
{
  uint64_t accessMask = ((uint32_t)((uint64_t)1 << GetEffectiveBitWidth())-1);

  return accessMask;
}

bool SvdRegister::AddToMap(SvdField *field, map<uint32_t, SvdField*> &map)
{
  const auto name = field->GetNameCalculated();
  const auto lineNo = field->GetLineNumber();
  const auto& accStr = SvdTypes::GetAccessType(field->GetEffectiveAccess());
  const auto offs = (uint32_t) field->GetOffset();
  const auto width = field->GetEffectiveBitWidth();

  for(uint32_t i=offs; i<offs+width; i++) {
    const auto f = map[i];
    if(f) {
      const auto nam = f->GetNameCalculated();
      const auto off = (uint32_t) f->GetOffset();
      const auto wid = f->GetEffectiveBitWidth();
      const auto& aStr = SvdTypes::GetAccessType(f->GetEffectiveAccess());

      LogMsg("M338", NAME(name), BITRANGE(offs+width-1, offs, true), ACCESS(accStr), NAME2(nam), BITRANGE2(off+wid-1, off, true), ACCESS2(aStr), LINE2(f->GetLineNumber()), lineNo);
      field->Invalidate();
      return false;
    }
    else {
      map[i] = field;
    }
  }

  return true;
}

bool SvdRegister::AddToMap(SvdField *field, map<string, SvdField*> &map)
{
  const auto name = field->GetNameCalculated();
  const auto lineNo = field->GetLineNumber();

  const auto f = map[name];
  if(f) {
    LogMsg("M336", LEVEL("Field"), NAME(name), LINE2(f->GetLineNumber()), lineNo);
    field->Invalidate();
  }
  else {
    map[name] = field;
  }

  return true;
}

bool SvdRegister::AddToMap(SvdEnum *enu, map<string, SvdEnum*> &map)
{
  const auto name = enu->GetNameCalculated();
  const auto lineNo = enu->GetLineNumber();

  const auto e = map[name];
  if(e) {
    auto nameComplete = enu->GetParent()->GetNameCalculated();
    if(!nameComplete.empty()) {
      nameComplete += ":";
    }
    nameComplete += name;

    LogMsg("M337", LEVEL("Enumerated Value"), NAME(nameComplete), LINE2(e->GetLineNumber()), lineNo);    // M545 as WARNING
    enu->Invalidate();
  }
  else {
    map[name] = enu;
  }

  return true;
}

bool SvdRegister::CheckEnumeratedValues()
{
  if(!m_enumContainer || !m_enumContainer->IsValid()) {
    return true;
  }

  const auto dim = GetDimension();
  if(!dim) {
    const auto name = m_enumContainer->GetNameCalculated();
    LogMsg("M229", NAME(name), m_enumContainer->GetLineNumber());
    return true;
  }

  const auto exp = dim->GetExpression();
  if(exp && exp->GetType()!= SvdTypes::Expression::ARRAY) {
    const auto name = m_enumContainer->GetNameCalculated();
    LogMsg("M229", NAME(name), m_enumContainer->GetLineNumber());
  }

  const auto dimElements = dim->GetDim();
  map<string, SvdEnum*> enumMap;

  const auto& childs = m_enumContainer->GetChildren();
  for(const auto child : childs) {
    SvdEnum* enu = dynamic_cast<SvdEnum*>(child);
    if(!enu || !enu->IsValid()) {
      continue;
    }

    const auto& enumName = enu->GetName();
    const auto enumValue = (uint32_t) enu->GetValue().u32;

    if(enumValue >= dimElements) {
      const auto regName = GetNameCalculated();
      const auto& svdLevelStr = GetSvdLevelStr(GetSvdLevel());
      auto nameComplete = enu->GetParent()->GetNameCalculated();
      if(!nameComplete.empty()) {
        nameComplete += ":";
      }
      nameComplete += enumName;
      LogMsg("M230", NAME(enumName), NUM(enumValue), LEVEL(svdLevelStr), NAME2(regName), NUM2(dimElements), enu->GetLineNumber());
      enu->Invalidate();
    }

    AddToMap(enu, enumMap);
  }

  return true;
}

bool SvdRegister::CheckFields(SvdItem* fields, uint32_t regWidth, const string& name)
{
  const auto& childs = fields->GetChildren();
  for(const auto child : childs) {
    SvdField* field = dynamic_cast<SvdField*>(child);
    if(!field || !field->IsValid()) {
      continue;
    }

    const auto dim = field->GetDimension();
    if(dim) {
      if(dim->GetExpression()->GetType() == SvdTypes::Expression::EXTEND) {
        CheckFields(dim, regWidth, name);
        return true;
      }
    }

    const auto fieldName = field->GetNameCalculated();
    const auto fieldOffs = (uint32_t)field->GetOffset();
    const auto fieldWidth = field->GetEffectiveBitWidth();
    const auto lineNo = field->GetLineNumber();

    if(fieldOffs + fieldWidth > regWidth) {
      LogMsg("M345", NAME(fieldName), BITRANGE(fieldOffs + fieldWidth -1, fieldOffs, true), NAME2(name), NUM(regWidth), lineNo);
      field->Invalidate();
    }

    const auto accType = field->GetEffectiveAccess();
    AddToMap(field, m_fieldsMap);

    switch(accType) {
      case SvdTypes::Access::READONLY:
        AddToMap(field, m_readMap);
        break;
      case SvdTypes::Access::WRITEONLY:
        AddToMap(field, m_writeMap);
        break;
      case SvdTypes::Access::READWRITE:
      case SvdTypes::Access::WRITEONCE:
      case SvdTypes::Access::READWRITEONCE:
        if(AddToMap(field, m_readMap)) {
          AddToMap(field, m_writeMap);
        }
        break;
      case SvdTypes::Access::UNDEF:
      default:
        break;
    }
  }

  uint32_t cnt = 0;
  for(const auto child : childs) {
    SvdField* field = dynamic_cast<SvdField*>(child);
    if(!field || !field->IsValid()) {
      continue;
    }

    cnt++;
  }

  if(!cnt) {
    fields->Invalidate();
  }

  return true;
}

bool SvdRegister::CheckItem()
{
  if(!IsValid()) {
    return true;
  }

  const auto name = GetNameCalculated();
  const auto lineNo = GetLineNumber();
  const auto regWidth = GetEffectiveBitWidth();
  const auto offset = GetOffset();
  const auto resetValue = GetResetValue();
  const auto resetMask = GetResetMask();

  if(offset == SvdItem::VALUE64_NOT_INIT) {
    const auto& svdLevelStr = GetSvdLevelStr(GetSvdLevel());
    LogMsg("M370", LEVEL(svdLevelStr), NAME(name), lineNo);
    Invalidate();
  }

  const auto maxRegValue = ((uint64_t)1 << regWidth) -1;
  if(resetValue > maxRegValue) {
    LogMsg("M382", LEVEL("Register"), NAME(name), NAME2("Reset Value"), HEXNUM(resetValue), NUM(regWidth), lineNo);
    SetResetValue(0);
  }
  if(resetMask > maxRegValue) {
    LogMsg("M382", LEVEL("Register"), NAME(name), NAME2("Reset Mask"), HEXNUM(resetMask), NUM(regWidth), lineNo);
    SetResetMask(0);
  }

  CheckEnumeratedValues();

  const auto dim = GetDimension();
  if(dim) {
    const auto addressBitsUnits = dim->GetAddressBitsUnits();
    const auto dimInc = dim->GetDimIncrement();
    const auto addressInc = addressBitsUnits * dimInc;

    if(addressInc < regWidth) {
      LogMsg("M366", NAME(name), NUM(regWidth), NUM2(dimInc), lineNo);
      Invalidate();
    }

    const auto expr = dim->GetExpression();
    if(expr) {
      const auto e = expr->GetType();
      if(e == SvdTypes::Expression::ARRAY) {
        if(dimInc*8 != regWidth) {
          LogMsg("M378", NAME(name), NUM(regWidth), NUM2(dimInc*8), lineNo);
          Invalidate();
        }
      }
    }
  }

  if(name.empty()) {
    return SvdItem::CheckItem();
  }

  const auto& alternate = GetAlternate();
  if(!alternate.empty()) {
    if(name.compare(alternate) == 0) {
      const string& svdLevelStr = GetSvdLevelStr(GetSvdLevel());
      LogMsg("M349", LEVEL(svdLevelStr), NAME(alternate), NAME2(name), lineNo);
    }
  }

  const auto fields = GetFieldContainer();
  if(fields) {
    CheckFields(fields, regWidth, name);
  }

  return SvdItem::CheckItem();
}
