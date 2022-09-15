/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdSauRegion.h"
#include "SvdUtils.h"
#include "XMLTree.h"
#include "ErrLog.h"
#include "SvdTypes.h"

using namespace std;


SvdSauRegionsConfig::SvdSauRegionsConfig(SvdItem* parent):
  SvdItem(parent),
  m_enabled(false),
  m_protectionWhenDisabled(SvdTypes::ProtectionType::UNDEF)
{
  SetSvdLevel(L_SvdSauRegionsConfig);
}

SvdSauRegionsConfig::~SvdSauRegionsConfig()
{
}

bool SvdSauRegionsConfig::Construct(XMLTreeElement* xmlElement)
{
  return SvdItem::Construct(xmlElement);
}

bool SvdSauRegionsConfig::ProcessXmlElement(XMLTreeElement* xmlElement)
{
	const auto& tag = xmlElement->GetTag();

  if(tag == "region") {
    const auto sauRegion = new SvdSauRegion(this);
    AddItem(sauRegion);
    return sauRegion->Construct(xmlElement);
	}

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdSauRegionsConfig::ProcessXmlAttributes(XMLTreeElement* xmlElement)
{
  const auto& attributes = xmlElement->GetAttributes();
  if(!attributes.empty()) {
    for(const auto& [tag, value] : attributes) {
      if(tag == "enabled") {
        if(!SvdUtils::ConvertNumber (value, m_enabled)) {
          SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
        }
      }

      if(tag == "protectionWhenDisabled") {
        if(!SvdUtils::ConvertSauProtectionStringType (value, m_protectionWhenDisabled, xmlElement->GetLineNumber())) {
          SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
        }
      }
    }
  }

  return SvdItem::ProcessXmlAttributes(xmlElement);
}

bool SvdSauRegionsConfig::CopyItem(SvdItem *from)
{
  return SvdItem::CopyItem(from);
}


SvdSauRegion::SvdSauRegion(SvdItem* parent):
  SvdItem(parent),
  m_enabled(true),
  m_base(SvdItem::VALUE32_NOT_INIT),
  m_limit(SvdItem::VALUE32_NOT_INIT),
  m_accessType(SvdTypes::SauAccessType::UNDEF)
{
  SetSvdLevel(L_SvdSauRegion);
}

SvdSauRegion::~SvdSauRegion()
{
}

bool SvdSauRegion::Calculate()
{
  return SvdItem::Calculate();
}

bool SvdSauRegion::Construct(XMLTreeElement* xmlElement)
{
  return SvdItem::Construct(xmlElement);
}

bool SvdSauRegion::ProcessXmlElement(XMLTreeElement* xmlElement)
{
	const auto& tag   = xmlElement->GetTag();
	const auto& value = xmlElement->GetText();

  if(tag == "base") {
    if(!SvdUtils::ConvertNumber(value, m_base)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "limit") {
    if(!SvdUtils::ConvertNumber(value, m_limit)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }
  else if(tag == "access") {
    if(!SvdUtils::ConvertSauAccessType (value, m_accessType, xmlElement->GetLineNumber())) {
        SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdSauRegion::ProcessXmlAttributes(XMLTreeElement* xmlElement)
{
  const auto& attributes = xmlElement->GetAttributes();
  if(!attributes.empty()) {
    for(const auto& [tag, value] : attributes) {
      if(tag == "enabled") {
        if(!SvdUtils::ConvertNumber (value, m_enabled)) {
          SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
        }
      }
      else if(tag == "name") {
        SetName(value);
      }
    }
  }

  return SvdItem::ProcessXmlAttributes(xmlElement);
}

bool SvdSauRegion::CopyItem(SvdItem *from)
{
  return SvdItem::CopyItem(from);
}

bool SvdSauRegion::CheckItem()
{
  if(!IsValid()) {
    return true;
  }

  return SvdItem::CheckItem();
}
