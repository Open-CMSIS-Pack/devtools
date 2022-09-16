/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdEnum_H
#define SvdEnum_H

#include "SvdTypes.h"
#include <set>


class SvdEnum;
class SvdEnumContainer : public SvdItem
{
public:
  SvdEnumContainer(SvdItem* parent);
  virtual ~SvdEnumContainer();

  virtual bool Construct(XMLTreeElement* xmlElement);
	virtual bool ProcessXmlElement(XMLTreeElement* xmlElement);
  virtual bool CopyItem(SvdItem *from);
  virtual bool CheckItem();

  bool                    SetDefaultValue       (SvdEnum* defaultValue) { m_defaultValue = defaultValue; return true; }
  SvdEnum*                GetDefaultValue       ()                      { return m_defaultValue; }
  bool                    SetHeaderEnumName     (const std::string& name);
  const std::string&      GetHeaderEnumName     ();

  SvdTypes::EnumUsage     GetUsage              ()  { return m_enumUsage; }
  SvdTypes::EnumUsage     GetEnumUsage          ()  { return m_enumUsage ; }
  SvdTypes::EnumUsage     GetEffectiveEnumUsage ();
  bool                    SetEnumUsage          (SvdTypes::EnumUsage enumUsage) { m_enumUsage = enumUsage; return true; }
  
protected:

private:
  SvdEnum*              m_defaultValue;
  SvdTypes::EnumUsage   m_enumUsage;
  std::string           m_headerEnumName;
};




class SvdEnum : public SvdItem
{
public:
  SvdEnum(SvdItem* parent);
  virtual ~SvdEnum();

  virtual bool Construct(XMLTreeElement* xmlElement);
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement);
  virtual bool ProcessXmlAttributes(XMLTreeElement* xmlElement);

  virtual bool Calculate();
  virtual bool CheckItem();
  virtual bool CopyItem(SvdItem *from);

  bool                      SetXBin(const std::set<uint32_t> &numbers)      { m_xBinNumbers = numbers; return true; }
  const std::set<uint32_t>& GetXBin()                                       { return m_xBinNumbers; }

  Value                   GetValue              ()               { return m_value     ; }
  bool                    IsDefault             ()               { return m_isDefault ; }
  bool                    SetIsDefault          (bool isDefault) { m_isDefault = isDefault; return true; }
  SvdTypes::EnumUsage     GetEffectiveEnumUsage ();
  bool                    SetValue              (uint64_t value);

protected:

private:
  bool                    m_isDefault;
  Value                   m_value;
  std::set<uint32_t>      m_xBinNumbers;
};

#endif // SvdEnum_H