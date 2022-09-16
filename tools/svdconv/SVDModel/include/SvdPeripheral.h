/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdPeripheral_H
#define SvdPeripheral_H

#include "SvdTypes.h"
#include "SvdCExpression.h"



class SvdPeripheral;
class SvdPeripheralContainer : public SvdItem
{
public:
  SvdPeripheralContainer(SvdItem* parent);
  virtual ~SvdPeripheralContainer();

  virtual bool Construct(XMLTreeElement* xmlElement);
	virtual bool ProcessXmlElement(XMLTreeElement* xmlElement);
  virtual bool CopyItem(SvdItem *from);
  virtual bool CheckItem();

protected:

private:
};


class SvdRegisterContainer;
class SvdRegister;
class SvdCluster;
class SvdAddressBlock;
class SvdInterrupt;
class SvdEnumContainer;
class SvdEnum;

class SvdPeripheral : public SvdItem
{
public:
  SvdPeripheral(SvdItem* parent);
  virtual ~SvdPeripheral();

  virtual bool            Construct                   (XMLTreeElement* xmlElement);
  virtual bool            ProcessXmlElement           (XMLTreeElement* xmlElement);
  virtual bool            ProcessXmlAttributes        (XMLTreeElement* xmlElement);

  virtual uint32_t        GetSize                     ();
  virtual uint32_t        SetSize                     (uint32_t size);
  virtual bool            CopyItem                    (SvdItem *from);
  virtual bool            Calculate                   ();
  virtual bool            CalculateDim                ();
  virtual std::string     GetNameCalculated           ();
  virtual bool            CheckItem                   ();
  virtual const std::string&   GetAlternate           () { return m_alternate;     }
  virtual const std::string&   GetPrependToName       () { return m_prependToName; }    // Prepend to Register name
  virtual const std::string&   GetAppendToName        () { return m_appendToName;  }    // Append  to Register name

  std::string             GetHeaderTypeName           ();
  bool                    AddToMap                    (SvdItem *item,     std::map<std::string, SvdItem*        >& map);
  bool                    AddToMap                    (SvdItem *item,     std::map<uint64_t,  std::list<SvdItem*> >& map);
  bool                    AddToMap                    (SvdRegister* reg,  std::map<uint32_t, std::list<SvdRegister*> >&regMap, std::map<uint32_t, std::list<SvdRegister*> > &alternateMap, bool bSilent = 0);
  bool                    AddToMap                    (SvdCluster* clust, std::map<uint32_t, std::list<SvdCluster* > >&clustMap, bool bSilent = 0);
  bool                    AddToMap_DisplayName        (SvdItem *item,     std::map<std::string, SvdItem*        >& map);
  bool                    CheckRegisters              (const std::list<SvdItem*> &childs);
  bool                    CheckClusterRegisters       (const std::list<SvdItem*> &childs);
  bool                    CheckRegisterAddress        (SvdRegister* reg,  const std::list<SvdAddressBlock*>& addrBlocks);
  bool                    CheckAddressBlocks          ();
  bool                    CheckAddressBlockOverlap    (SvdAddressBlock* addrBlock);
  bool                    CheckAddressBlockAddrSpace  (SvdAddressBlock* addrBlock);
  bool                    SortAddressBlocks           (std::map<uint64_t, SvdAddressBlock*>& addrBlocksSort);
  bool                    CopyMergedAddressBlocks     (std::map<uint64_t, SvdAddressBlock*>& addrBlocksSort);
  bool                    MergeAddressBlocks          ();
  bool                    SearchAlternateMap          (SvdRegister *reg, std::map<uint32_t, std::list<SvdRegister*> > &alternateMap);
  SvdTypes::Access        CalcAccessSVDConvV2         (SvdRegister* reg);
  SvdTypes::SvdConvV2accType ConvertAccessToSVDConvV2 (SvdTypes::Access access);
  SvdTypes::Access        ConvertAccessFromSVDConvV2  (SvdTypes::SvdConvV2accType access);
  SvdRegisterContainer*   GetRegisterContainer        () const;
  void                    AddAddressBlock             (SvdAddressBlock* addrBlock);
  std::list<SvdAddressBlock*>& GetAddressBlock        ();
  void                    ClearAddressBlock           () { m_addressBlock.clear(); }
  void                    AddInterrupt                (SvdInterrupt* interrupt);
  std::list<SvdInterrupt*>& GetInterrupt              ();
  void                    ClearInterrupt              () { m_interrupt.clear(); }
  bool                    CopyAddressBlocks           (SvdPeripheral *from);
  bool                    CopyRegisterContainer       (SvdPeripheral *from);
  bool                    CheckEnumeratedValues       ();
  bool                    CalcDisableCondition        ();
  bool                    AddToMap                    (SvdEnum *enu, std::map<std::string, SvdEnum*> &map);
  bool                    CalculateMaxPaddingWidth    ();

  const std::string&      GetVersion                  () { return m_version;          }
  const std::string&      GetGroupName                () { return m_groupName;        }
  const std::string&      GetHeaderStructName         () { return m_headerStructName; }
  SvdCExpression*         GetDisableCondition         () { return m_disableCondition; }
  uint64_t                GetAddress                  () { return m_address.u64;      }
  uint64_t                GetResetValue               () { return m_resetValue;       }
  uint64_t                GetResetMask                () { return m_resetMask;        }
  SvdTypes::Access        GetAccess                   () { return m_access;           }
  bool                    GetAddressValid             () { return m_address.bValid;   }
  bool                    GetHasAnnonUnions           () { return m_hasAnnonUnions;   }
  SvdEnumContainer*       GetEnumContainer            () { return m_enumContainer;    }
  bool                    SetHasAnnonUnions           () { m_hasAnnonUnions   = true            ;  return true; }

  bool                    SetVersion                  ( const std::string&  version         )  { m_version          = version         ;  return true; }
  bool                    SetGroupName                ( const std::string&  groupName       )  { m_groupName        = groupName       ;  return true; }
  bool                    SetHeaderStructName         ( const std::string&  headerStructName)  { m_headerStructName = headerStructName;  return true; }
  bool                    SetAlternate                ( const std::string&  alternate       )  { m_alternate        = alternate       ;  return true; }
  bool                    SetPrependToName            ( const std::string&  prependToName   )  { m_prependToName    = prependToName   ;  return true; }
  bool                    SetAppendToName             ( const std::string&  appendToName    )  { m_appendToName     = appendToName    ;  return true; }
  bool                    SetDisableCondition         ( SvdCExpression*     disableCondition)  { m_disableCondition = disableCondition;  return true; }
  bool                    SetAddress                  ( uint64_t            address         )  { m_address.u64      = address         ;  m_address.bValid = true; return true; }
  bool                    SetResetValue               ( uint64_t            resetValue      )  { m_resetValue       = resetValue      ;  return true; }
  bool                    SetResetMask                ( uint64_t            resetMask       )  { m_resetMask        = resetMask       ;  return true; }
  bool                    SetAccess                   ( SvdTypes::Access    access          )  { m_access           = access          ;  return true; }

protected:

private:
  SvdEnumContainer*           m_enumContainer;
  SvdCExpression*             m_disableCondition;
  bool                        m_hasAnnonUnions;
  uint32_t                    m_calcSize;
  uint64_t                    m_resetValue;
  uint64_t                    m_resetMask;
  SvdTypes::Access            m_access;
  Value                       m_address;
  std::list<SvdAddressBlock*> m_addressBlock;
  std::list<SvdInterrupt*>    m_interrupt;
  std::string                 m_version;
  std::string                 m_groupName;
  std::string                 m_headerStructName;
  std::string                 m_alternate;
  std::string                 m_prependToName;
  std::string                 m_appendToName;

  std::map<std::string, SvdItem*>               m_regsMap;
  std::map<std::string, SvdItem*>               m_regsMap_displayName;
  std::map<uint32_t, std::list<SvdRegister*> >  m_readMap;
  std::map<uint32_t, std::list<SvdRegister*> >  m_writeMap;
  std::map<uint32_t, std::list<SvdRegister*> >  m_readWriteMap;
  std::map<uint32_t, std::list<SvdCluster*>  >  m_clustMap;
  std::map<uint64_t, std::list<SvdItem*>   >    m_allMap;
};

#endif // SvdPeripheral_H
