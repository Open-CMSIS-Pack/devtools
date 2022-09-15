/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdAddressBlock_H
#define SvdAddressBlock_H

#include "SvdTypes.h"

class SvdAddressBlock : public SvdItem
{
public:
  SvdAddressBlock(SvdItem* parent);
  virtual ~SvdAddressBlock();

  virtual bool Construct(XMLTreeElement* xmlElement);
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement);
  virtual bool ProcessXmlAttributes(XMLTreeElement* xmlElement);

  virtual bool CopyItem(SvdItem *from);
  virtual bool CheckItem();

  uint32_t GetOffset() { return m_offset; }
  bool SetOffset(uint32_t offset) { m_offset = offset; return true; }

  uint32_t GetSize() { return m_size; }
  bool SetSize(uint32_t size) { m_size = size; return true; }

  SvdTypes::AddrBlockUsage GetUsage () { return m_usage ; }
  bool SetUsage (SvdTypes::AddrBlockUsage usage ) { m_usage = usage; return true; }

  bool SetMerged() { m_bMerged = true; return true; }
  bool IsMerged() { return m_bMerged; }

  bool SetCopied() { m_bCopied = true; return true; }
  bool IsCopied() { return m_bCopied; }

protected:

private:
  bool      m_bMerged;
  bool      m_bCopied;
  uint32_t  m_offset;
  uint32_t  m_size;
  SvdTypes::AddrBlockUsage  m_usage;
};

#endif // SvdAddressBlock_H
