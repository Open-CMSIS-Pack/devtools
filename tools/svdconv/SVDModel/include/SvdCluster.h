/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdCluster_H
#define SvdCluster_H

#include "SvdTypes.h"

#include <string>


class SvdWriteConstraint;
class SvdRegister;
class SvdEnumContainer;
class SvdEnum;

class SvdCluster : public SvdItem
{
public:
  SvdCluster(SvdItem* parent);
  virtual ~SvdCluster();

  virtual bool                        Construct               (XMLTreeElement* xmlElement);
  virtual bool                        ProcessXmlElement       (XMLTreeElement* xmlElement);
  virtual bool                        ProcessXmlAttributes    (XMLTreeElement* xmlElement);

  virtual bool                        CopyItem                (SvdItem *from);
  virtual bool                        Calculate               ();
  virtual bool                        CalculateDim            ();
  virtual uint64_t                    GetAddress              ()                                                    { return m_offset;               }     // needed for absolute address calculation
  virtual uint32_t                    GetSize                 ();
  virtual uint32_t                    SetSize                 (uint32_t size);
  virtual std::string                 GetNameCalculated       ();
  virtual bool                        CheckItem               ();
  
  std::string                         GetHeaderTypeName       ();
  std::string                         GetHeaderTypeNameHierarchical();
  const std::string&                  GetAlternate            ()                                                    { return m_alternate;           }
  const std::string&                  GetHeaderStructName     ()                                                    { return m_headerStructName;    }
  uint64_t                            GetOffset               ()                                                    { return m_offset;              }
  uint64_t                            GetResetValue           ()                                                    { return m_resetValue;          }
  uint64_t                            GetResetMask            ()                                                    { return m_resetMask;           }
  SvdTypes::Access                    GetAccess               ()                                                    { return m_access;              }
  SvdTypes::ModifiedWriteValue        GetModifiedWriteValues  ()                                                    { return m_modifiedWriteValues; }
  SvdTypes::ReadAction                GetReadAction           ()                                                    { return m_readAction;          }
  SvdEnumContainer*                   GetEnumContainer        ()                                                    { return m_enumContainer;        }
  bool                                SetAlternate            (const std::string             &alternate         )   { m_alternate           = alternate          ; return true; }
  bool                                SetHeaderStructName     (const std::string             &headerStructName  )   { m_headerStructName    = headerStructName   ; return true; }
  bool                                SetOffset               (uint64_t                         offset          )   { m_offset              = offset             ; return true; }
  bool                                SetResetValue           (uint64_t                         resetValue      )   { m_resetValue          = resetValue         ; return true; }
  bool                                SetResetMask            (uint64_t                         resetMask       )   { m_resetMask           = resetMask          ; return true; }
  bool                                SetAccess               (SvdTypes::Access             access              )   { m_access              = access             ; return true; }
  bool                                SetModifiedWriteValues  (SvdTypes::ModifiedWriteValue modifiedWriteValues )   { m_modifiedWriteValues = modifiedWriteValues; return true; }
  bool                                SetReadAction           (SvdTypes::ReadAction         readAction          )   { m_readAction          = readAction         ; return true; }
  bool                                CheckEnumeratedValues   ();
  bool                                AddToMap                (SvdEnum *enu, std::map<std::string, SvdEnum*> &map);
  bool                                CalculateMaxPaddingWidth();

protected:

private:
  SvdEnumContainer               *m_enumContainer;
  uint32_t                        m_calcSize;
  uint64_t                        m_offset;
  uint64_t                        m_resetValue;
  uint64_t                        m_resetMask;
  SvdTypes::Access                m_access;
  SvdTypes::ModifiedWriteValue    m_modifiedWriteValues;
  SvdTypes::ReadAction            m_readAction;
  std::string                     m_alternate;
  std::string                     m_headerStructName;
};

#endif // SvdCluster_H
