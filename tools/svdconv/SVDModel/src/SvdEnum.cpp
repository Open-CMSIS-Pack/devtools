/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdEnum.h"
#include "SvdUtils.h"
#include "XMLTree.h"
#include "ErrLog.h"

using namespace std;


SvdEnumContainer::SvdEnumContainer(SvdItem* parent):
  SvdItem(parent),
  m_defaultValue(0),
  m_enumUsage(SvdTypes::EnumUsage::UNDEF)
{
  const auto svdLevel = parent->GetSvdLevel();
  if(svdLevel == L_Peripheral || svdLevel == L_Register || svdLevel == L_Cluster) {
    SetSvdLevel(L_DimArrayIndex);
  }
  else {
    SetSvdLevel(L_EnumeratedValues);
  }
}

SvdEnumContainer::~SvdEnumContainer()
{
}

bool SvdEnumContainer::Construct(XMLTreeElement* xmlElement)
{
  return SvdItem::Construct(xmlElement);
}

bool SvdEnumContainer::ProcessXmlElement(XMLTreeElement* xmlElement)
{
	const auto& tag   = xmlElement->GetTag();
  const auto& value = xmlElement->GetText();

  if(tag == "enumeratedValue") {
    const auto enu = new SvdEnum(this);
    AddItem(enu);
    return enu->Construct(xmlElement);
	}
  else if(tag == "usage") {
    if(!SvdUtils::ConvertEnumUsage(value, m_enumUsage, xmlElement->GetLineNumber())) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "headerEnumName") {
    SetHeaderEnumName(value);
    return true;
  }

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdEnumContainer::SetHeaderEnumName(const string& name)
{
  m_headerEnumName = name;

  return true;
}

const string& SvdEnumContainer::GetHeaderEnumName()
{
  return m_headerEnumName;
}

SvdTypes::EnumUsage SvdEnumContainer::GetEffectiveEnumUsage()
{
  auto enumUsage = GetEnumUsage();
  if(enumUsage == SvdTypes::EnumUsage::UNDEF) {
    enumUsage = SvdTypes::EnumUsage::READWRITE;
  }

  return enumUsage;
}

bool SvdEnumContainer::CopyItem(SvdItem *from)
{
  const auto pFrom = dynamic_cast<SvdEnumContainer*>(from);

  if(pFrom) {
    const auto enumUsage = GetEnumUsage();
    if(enumUsage == SvdTypes::EnumUsage::UNDEF) {
      SetEnumUsage(pFrom->GetEnumUsage());
    }
  }

  return SvdItem::CopyItem(from);
}

bool SvdEnumContainer::CheckItem()
{
  const auto& headerEnumName = GetHeaderEnumName();
  const auto& name           = GetNameCalculated();
  const auto  lineNo         = GetLineNumber();

  if(!headerEnumName.empty() && headerEnumName == name) {
    const string& svdLevelStr = GetSvdLevelStr(GetSvdLevel());
    LogMsg("M318", LEVEL(svdLevelStr), TAG("headerEnumName"), NAME(name), lineNo);
  }

  return SvdItem::CheckItem();
}

SvdEnum::SvdEnum(SvdItem* parent):
  SvdItem(parent),
  m_isDefault(false)
{
  SetSvdLevel(L_EnumeratedValue);
}

SvdEnum::~SvdEnum()
{
}

bool SvdEnum::Construct(XMLTreeElement* xmlElement)
{
  return SvdItem::Construct(xmlElement);
}

bool SvdEnum::ProcessXmlElement(XMLTreeElement* xmlElement)
{
	const auto& tag   = xmlElement->GetTag();
  const auto& value = xmlElement->GetText();

  if(tag == "value") {
    set<uint32_t> numbers;
    if(!SvdUtils::ConvertNumberXBin(value, numbers)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
      Invalidate();
    }
    else {
      if(numbers.empty()) {
        SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
      }
      else if(numbers.size() == 1) {
        SetValue(*numbers.begin());
      }
      else {
        SetXBin(numbers);
      }
    }
    return true;
  }
  else if(tag == "isDefault") {
    const auto svdLevel = GetParent()->GetSvdLevel();
    if(svdLevel == L_DimArrayIndex) {
      const auto& svdLevelStr = GetSvdLevelStr(svdLevel);
      LogMsg("M231", LEVEL(svdLevelStr), xmlElement->GetLineNumber());
      m_isDefault = true;
      Invalidate();
      return true;
    }

    if(!SvdUtils::ConvertNumber(value, m_isDefault)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }

    if(!m_isDefault) {
      //LogMsg("M232", xmlElement->GetLineNumber());
      //Invalidate();
    } else {
      const auto parent = (SvdEnumContainer*)(GetParent());
      if(parent) {
        parent->SetDefaultValue(this);
      }
    }
    return true;
  }

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdEnum::ProcessXmlAttributes(XMLTreeElement* xmlElement)
{
  return SvdItem::ProcessXmlAttributes(xmlElement);
}

bool SvdEnum::Calculate()
{
  const auto& numbers = GetXBin();
  if(!numbers.empty()) {
    const auto parent = GetParent();
    const auto name = GetName();    // Must not use "string&" here because "m_name" gets changed by SetName(n)
    auto it = numbers.begin();      // Set first Element
    SetValue(*it);
    string n = name;
    n += "_";
    n += SvdUtils::CreateDecNum(*it);
    SetName(n);

    it++;
    if(parent) {
      for(; it != numbers.end(); it++) {
        const auto enu = new SvdEnum(parent);
        parent->AddItem(enu);
        enu->CopyItem(this);
        enu->SetValue(*it);

        string n2 = name;
        n2 += "_";
        n2 += SvdUtils::CreateDecNum(*it);
        enu->SetName(n2);
      }
    }
  }

  return SvdItem::Calculate();
}

bool SvdEnum::SetValue(uint64_t value)
{
  m_value.u64 = value;
  m_value.bValid = true;

  return true;
}

bool SvdEnum::CopyItem(SvdItem *from)
{
  const auto pFrom = (SvdEnum*) from;

  bool valValid  = GetValue ().bValid;
  bool isDefault = IsDefault();

  if(!valValid) {
    if(pFrom->GetValue().bValid) {
      SetValue(pFrom->GetValue().u32);
    }
  }

  if(isDefault == 0 ) {
    SetIsDefault(pFrom->IsDefault());
  }

  return SvdItem::CopyItem(from);
}

SvdTypes::EnumUsage SvdEnum::GetEffectiveEnumUsage()
{
  auto enumUsage = SvdTypes::EnumUsage::UNDEF;
  if(enumUsage == SvdTypes::EnumUsage::UNDEF) {
    const auto enumCont = dynamic_cast<SvdEnumContainer*>(GetParent());
    if(!enumCont) {
      return SvdTypes::EnumUsage::READWRITE;
    }

    enumUsage = enumCont->GetEnumUsage();
    if(enumUsage == SvdTypes::EnumUsage::UNDEF) {
      enumUsage = SvdTypes::EnumUsage::READWRITE;
    }
  }

  return enumUsage;
}

bool SvdEnum::CheckItem()
{
  if(!IsValid()) {
    return true;
  }

  const auto& name = GetName();
  if(name.empty()) {
    return SvdItem::CheckItem();
  }

  const auto lineNo = GetLineNumber();
  const auto val = GetValue();
  bool isDefault = IsDefault();

  if(!isDefault && !val.bValid) {
    LogMsg("M369", NAME(name), lineNo);
    Invalidate();
  }

  if(val.bValid) {
    auto valStr = SvdUtils::CreateDecNum(val.u32);
    if(name == valStr) {
      LogMsg("M307", NAME(name), lineNo);
    }
  }

  return SvdItem::CheckItem();
}
