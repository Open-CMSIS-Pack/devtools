/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdSauRegion_H
#define SvdSauRegion_H

#include "SvdTypes.h"


class SvdSauRegion;
class SvdSauRegionsConfig : public SvdItem
{
public:
  SvdSauRegionsConfig(SvdItem* parent);
  virtual ~SvdSauRegionsConfig();

	virtual bool ProcessXmlElement(XMLTreeElement* xmlElement);
  virtual bool ProcessXmlAttributes(XMLTreeElement* xmlElement);
  virtual bool Construct(XMLTreeElement* xmlElement);
  virtual bool CopyItem(SvdItem *from);

  bool GetEnabled() { return m_enabled; }
  SvdTypes::ProtectionType GetProtectionWhenDisabled() { return m_protectionWhenDisabled; }
 
protected:

private:
  bool                       m_enabled;
  SvdTypes::ProtectionType   m_protectionWhenDisabled;
};


class SvdSauRegion : public SvdItem
{
public:
  SvdSauRegion(SvdItem* parent);
  virtual ~SvdSauRegion();

  virtual bool Construct(XMLTreeElement* xmlElement);
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement);
  virtual bool ProcessXmlAttributes(XMLTreeElement* xmlElement);

  virtual bool CopyItem(SvdItem *from);
  virtual bool Calculate();
  virtual bool CheckItem();

  bool      GetEnabled    () { return m_enabled   ; }
  uint32_t  GetBase       () { return m_base      ; }
  uint32_t  GetLimit      () { return m_limit     ; }
  SvdTypes::SauAccessType GetAccessType () { return m_accessType; }

protected:

private:
  bool      m_enabled;
  uint32_t  m_base;
  uint32_t  m_limit;
  SvdTypes::SauAccessType m_accessType;
};

#endif // SvdSauRegion_H
