/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdRegister_H
#define SvdRegister_H

#include "SvdTypes.h"


class SvdRegister;
class SvdCluster;
class SvdRegisterContainer : public SvdItem
{
public:
  SvdRegisterContainer(SvdItem* parent);
  virtual ~SvdRegisterContainer();

  virtual bool Construct(XMLTreeElement* xmlElement);
	virtual bool ProcessXmlElement(XMLTreeElement* xmlElement);
  virtual bool CopyItem(SvdItem *from);

protected:

private:
};


class SvdFieldContainer;
class SvdField;
class SvdEnumContainer;
class SvdEnum;
class SvdWriteConstraint;

class SvdRegister : public SvdItem
{
public:
  SvdRegister(SvdItem* parent);
  virtual ~SvdRegister();

  bool Construct(XMLTreeElement* xmlElement);
  bool ProcessXmlElement(XMLTreeElement* xmlElement);
  bool ProcessXmlAttributes(XMLTreeElement* xmlElement);
  virtual bool Calculate();
  virtual bool CalculateDim();

  virtual bool CopyItem(SvdItem *from);
  virtual bool CheckItem();

  bool AddToMap(SvdField *field, std::map<uint32_t,    SvdField*> &map);
  bool AddToMap(SvdField *field, std::map<std::string, SvdField*> &map);

  std::string                   GetHeaderTypeName();
  SvdFieldContainer*            GetFieldContainer() const;

  virtual uint64_t              GetAddress              () { return m_offset;               }     // needed for absolute address calculation
  virtual const std::string&    GetAlternateGroup       () { return m_alternateGroup;       }
  virtual std::string           GetNameCalculated       ();
                                                           
  const std::string&            GetAlternate            () { return m_alternate;            }
  const std::string&            GetDataType             () { return m_dataType;             }
  uint64_t                      GetOffset               () { return m_offset;               }
  uint64_t                      GetResetValue           () { return m_resetValue;           }
  uint64_t                      GetResetMask            () { return m_resetMask;            }
  SvdTypes::Access              GetAccess               () { return m_access;               }
  SvdTypes::ModifiedWriteValue  GetModifiedWriteValue   () { return m_modifiedWriteValues;  }
  SvdTypes::ReadAction          GetReadAction           () { return m_readAction;           }
  SvdEnumContainer*             GetEnumContainer        () { return m_enumContainer;        }
  bool                          HasValidFields          () {return m_hasValidFields;        }
  bool                          SetNoValidFields        () { m_hasValidFields = false; return true; }

  bool                          SetAlternate            (const std::string& alternate                   ) { m_alternate           = alternate;          return true; }
  bool                          SetAlternateGroup       (const std::string& alternateGroup              ) { m_alternateGroup      = alternateGroup;     return true; }
  bool                          SetDataType             (const std::string& dataType                    ) { m_dataType            = dataType;           return true; }
  bool                          SetOffset               (uint64_t offset                                ) { m_offset              = offset;             return true; }
  bool                          SetResetValue           (uint64_t val                                   ) { m_resetValue          = val;                return true; }
  bool                          SetResetMask            (uint64_t val                                   ) { m_resetMask           = val;                return true; }
  bool                          SetAccess               (SvdTypes::Access             access            ) { m_access              = access;             return true; }
  bool                          SetModifiedWriteValues  (SvdTypes::ModifiedWriteValue modifiedWriteValue) { m_modifiedWriteValues = modifiedWriteValue; return true; }
  bool                          SetReadAction           (SvdTypes::ReadAction         readAction        ) { m_readAction          = readAction;         return true; }
  std::string                   GetHeaderFileName       ();
  bool                          CalcAccessMask          ();
  uint64_t                      GetAccessMaskRead       ();
  uint64_t                      GetAccessMaskWrite      ();
  uint64_t                      GetAccessMask           ();
  SvdTypes::Access              GetAccessCalculated     ();
  bool                          CheckEnumeratedValues   ();
  bool                          AddToMap                (SvdEnum *enu, std::map<std::string, SvdEnum*> &map);
  bool                          CheckFields             (SvdItem* fields, uint32_t regWidth, const std::string& name);

protected:

private:
  SvdWriteConstraint*           m_svdWriteConstraint;
  SvdEnumContainer*             m_enumContainer;
  bool                          m_hasValidFields;
  bool                          m_accessMaskValid;
  uint64_t                      m_offset;
  uint64_t                      m_resetValue;
  uint64_t                      m_resetMask;
  uint64_t                      m_accessMaskRead;
  uint64_t                      m_accessMaskWrite;
  SvdTypes::Access              m_access;
  SvdTypes::ModifiedWriteValue  m_modifiedWriteValues;
  SvdTypes::ReadAction          m_readAction;
  std::string                   m_alternate;
  std::string                   m_alternateGroup;
  std::string                   m_dataType;

  std::map<std::string, SvdField*>  m_fieldsMap;
  std::map<uint32_t,    SvdField*>  m_readMap;
  std::map<uint32_t,    SvdField*>  m_writeMap;
};

#endif // SvdRegister_H
