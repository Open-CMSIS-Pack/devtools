/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdAddressBlock.h"
#include "SvdUtils.h"
#include "XMLTree.h"
#include "ErrLog.h"

using namespace std;

static constexpr uint32_t MAX_VALUE = 0x1000000;        // detect miscalculated numbers


SvdAddressBlock::SvdAddressBlock(SvdItem* parent):
  SvdItem(parent),
  m_bMerged(false),
  m_bCopied(false),
  m_offset(SvdItem::VALUE32_NOT_INIT),
  m_size(SvdItem::VALUE32_NOT_INIT),
  m_usage(SvdTypes::AddrBlockUsage::UNDEF)
{
  SetSvdLevel(L_AddressBlock);
}

SvdAddressBlock::~SvdAddressBlock()
{
}

bool SvdAddressBlock::Construct(XMLTreeElement* xmlElement)
{
  return SvdItem::Construct(xmlElement);
}

bool SvdAddressBlock::ProcessXmlElement(XMLTreeElement* xmlElement)
{
	const auto& tag   = xmlElement->GetTag();
  const auto& value = xmlElement->GetText();
  const auto lineNo = xmlElement->GetLineNumber();

  if(tag == "offset") {
    uint64_t num = 0;
    if(!SvdUtils::ConvertNumber(value, num)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }

    if(num > MAX_VALUE) {
      LogMsg("M360", TAG(tag), HEXNUM(num), HEXNUM2(MAX_VALUE), lineNo);
      //Invalidate();     // allow huge addressBlocks for SVDConv V2 compatibility
      m_offset = (uint32_t)num;
    }
    else {
      m_offset = (uint32_t)num;
    }

    return true;
  }
  else if(tag == "size") {
    uint64_t num = 0;
    if(!SvdUtils::ConvertNumber(value, num)) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    if(num > MAX_VALUE) {
      LogMsg("M360", TAG(tag), HEXNUM(num), HEXNUM2(MAX_VALUE), lineNo);
      //Invalidate();     // allow huge addressBlocks for SVDConv V2 compatibility
      m_size = (uint32_t)num;
    }
    else {
      m_size = (uint32_t)num;
    }

    SetModified();
    return true;
  }
  else if(tag == "usage") {
    if(!SvdUtils::ConvertAddrBlockUsage(value, m_usage, xmlElement->GetLineNumber())) {
      SvdUtils::CheckParseError(tag, value, xmlElement->GetLineNumber());
    }
    return true;
  }

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdAddressBlock::ProcessXmlAttributes(XMLTreeElement* xmlElement)
{
  return SvdItem::ProcessXmlAttributes(xmlElement);
}

bool SvdAddressBlock::CopyItem(SvdItem *from)
{
  const auto pFrom  = (SvdAddressBlock*) from;
  const auto offset = GetOffset();
  const auto size   = GetSize();
  const auto usage  = GetUsage();

  if(offset == SvdItem::VALUE32_NOT_INIT) {
    SetOffset(pFrom->GetOffset());
  }
  
  if(size == SvdItem::VALUE32_NOT_INIT) {
    SetSize  (pFrom->GetSize());
  }

  if(usage == SvdTypes::AddrBlockUsage::UNDEF) {
    SetUsage (pFrom->GetUsage());
  }

  SetCopied();

  return SvdItem::CopyItem(from);
}

bool SvdAddressBlock::CheckItem()
{
  const auto lineNo = GetLineNumber();

  if(!IsValid()) {
    return true;
  }

  const auto offset = GetOffset();
  const auto size   = GetSize();
  const auto usage = GetUsage();

  if(offset == SvdItem::VALUE32_NOT_INIT || size == SvdItem::VALUE32_NOT_INIT) {
    LogMsg("M314", lineNo);
    Invalidate();
  }

  if(!size) {
    LogMsg("M315", lineNo);
    Invalidate();
  }

  if(usage == SvdTypes::AddrBlockUsage::UNDEF) {
    LogMsg("M359", lineNo);
    Invalidate();
  }

  return SvdItem::CheckItem();
}
