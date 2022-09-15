/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdField_H
#define SvdField_H

#include "SvdTypes.h"


// Configuration
#define FIELD_MAX_BITWIDTH        64
#define FIELD_MAX_OFFSET          (FIELD_MAX_BITWIDTH -1)



class SvdField;
class SvdFieldContainer : public SvdItem
{
public:
  SvdFieldContainer(SvdItem* parent);
  virtual ~SvdFieldContainer();
  virtual bool Construct(XMLTreeElement* xmlElement);
	virtual bool ProcessXmlElement(XMLTreeElement* xmlElement);
  virtual bool CopyItem(SvdItem *from);

protected:

private:
};


class SvdEnumContainer;
class SvdEnum;
class SvdWriteConstraint;
class SvdField : public SvdItem
{
public:
  SvdField(SvdItem* parent);
  virtual ~SvdField();

  virtual bool Calculate();

  bool Construct(XMLTreeElement* xmlElement);
  bool ProcessXmlElement(XMLTreeElement* xmlElement);
  bool ProcessXmlAttributes(XMLTreeElement* xmlElement);

  const std::list<SvdEnumContainer*>&  GetEnumContainer() const { return (const std::list<SvdEnumContainer*>&)GetChildren(); }

  virtual bool  CopyItem(SvdItem *from);
  bool          GetValuesDescriptionString(std::string &longDescr);
  virtual bool  CheckItem();
  virtual bool  CalculateDim();

  bool AddToMap(SvdEnum *enu,           std::map<std::string, SvdEnum*> &map);
  bool AddToMap(SvdEnumContainer *enu,  std::map<std::string, SvdEnumContainer*> &map);
  bool AddToMap(SvdEnum *enu,           std::map<uint32_t, SvdEnum*> &map);

  virtual uint64_t                  GetAddress              () { return m_offset;               }

  uint64_t                          GetOffset               () { return m_offset;               }
  SvdTypes::Access                  GetAccess               () { return m_access;               }
  SvdTypes::ModifiedWriteValue      GetModifiedWriteValue   () { return m_modifiedWriteValues;  }
  SvdTypes::ReadAction              GetReadAction           () { return m_readAction;           }

  bool  SetOffset               (uint64_t                     offset             )  { m_offset              = offset             ; return true; }
  bool  SetAccess               (SvdTypes::Access             access             )  { m_access              = access             ; return true; }
  bool  SetModifiedWriteValue   (SvdTypes::ModifiedWriteValue modifiedWriteValues)  { m_modifiedWriteValues = modifiedWriteValues; return true; }
  bool  SetReadAction           (SvdTypes::ReadAction         readAction         )  { m_readAction          = readAction         ; return true; }

  uint32_t   GetLsb() { return m_lsb;                  }
  uint32_t   GetMsb() { return m_msb;                  }
  bool  SetLsb(uint32_t lsb) { m_lsb = lsb; return true; }
  bool  SetMsb(uint32_t msb) { m_msb = msb; return true; }

protected:

private:
  SvdWriteConstraint             *m_writeConstraint;
  uint32_t                        m_lsb;
  uint32_t                        m_msb;
  uint64_t                        m_offset;
  SvdTypes::Access                m_access;
  SvdTypes::ModifiedWriteValue    m_modifiedWriteValues;
  SvdTypes::ReadAction            m_readAction;
};

#endif // SvdField_H
