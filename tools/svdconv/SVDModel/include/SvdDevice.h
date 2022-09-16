/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdDevice_H
#define SvdDevice_H

#include "SvdItem.h"
#include "SvdTypes.h"
#include "SvdCExpression.h"

#include <map>
#include <string>

class SvdPeripheralContainer;
class SvdPeripheral;
class SvdRegister;
class SvdEnumContainer;
class SvdCpu;
class SvdInterrupt;
class SvdCluster;
class XMLTreeElement;
class SvdAddressBlock;

class SvdDevice : public SvdItem
{
public:
  SvdDevice(SvdItem* parent);
  virtual ~SvdDevice();

	virtual bool ProcessXmlElement    (XMLTreeElement* xmlElement);
  virtual bool ProcessXmlAttributes (XMLTreeElement* xmlElement);

  virtual SvdDevice* GetDevice() const;
  virtual const std::string& GetHeaderDefinitionsPrefix() { return m_headerDefinitionsPrefix; }
  virtual bool Construct(XMLTreeElement* xmlElement);
  virtual bool Calculate();
  virtual bool CopyItem(SvdItem *from) { return false; }
  virtual bool            CheckItem();

  SvdPeripheralContainer* GetPeripheralContainer() const;
  bool                    CheckForItemsPeri             (const std::list<SvdItem*> &childs);
  bool                    CheckForItemsCluster          (const std::list<SvdItem*> &childs);
  bool                    CheckForItemsRegister         (SvdRegister* reg);
  bool                    CheckPeripherals              (const std::list<SvdItem*> &childs);
  bool                    CheckInterrupts               (const std::map<uint32_t, SvdInterrupt*>& interrupts);
  bool                    AddToMap                      (SvdItem*       item);
  bool                    AddToMap                      (SvdPeripheral* peri);
  bool                    AddToMap                      (SvdCluster*    clust);
  bool                    AddToMap                      (SvdEnumContainer* enumCont);
  bool                    AddToMap                      (SvdItem *item, const std::string& name, const std::string& tagUsed, std::map<std::string, SvdItem*>& map);
  bool                    AddToMap                      (SvdPeripheral* peri, std::map<uint32_t, std::list<SvdPeripheral*> > &map, bool bSilent = 0);
  bool                    AddClusterNames               (const std::list<SvdItem*>& childs);
  bool                    CheckPeripheralOverlap        (const std::map<std::string, SvdItem*>& perisMap);
  bool                    CheckAddressBlockOverlap      (SvdPeripheral* peri, SvdAddressBlock* addrBlock, const std::map<std::string, SvdItem*>& perisMap);
  bool                    CheckEnumContainerNames       (SvdRegister* reg);
  SvdCpu*                 GetCpu                        ()  { return m_cpu; }
  bool                    AddInterrupt                  (SvdInterrupt* interrupt);
  bool                    CreateInterrupts              ();
  const std::map<uint32_t, SvdInterrupt*>& GetInterruptList  ()  { return m_interruptList; }
  bool                    CreatePeripheralTypes         ();
  bool                    CreateClusters                ();
  bool                    GatherClusters                (SvdItem *item);

  const std::list<SvdCluster*>&     GetClusterList      () { return m_clusterList;     }
  const std::list<SvdPeripheral*>&  GetPeripheralList   () { return m_peripheralList;  }

  const std::string&  GetFileName                       () { return m_fileName               ; }
  const std::string&  GetVendor                         () { return m_vendor                 ; }
  const std::string&  GetVendorId                       () { return m_vendorId               ; }
  const std::string&  GetSeries                         () { return m_series                 ; }
  const std::string&  GetVersion                        () { return m_version                ; }
  const std::string&  GetLicenseText                    () { return m_licenseText            ; }
  const std::string&  GetHeaderSystemFilename           () { return m_headerSystemFilename   ; }
  uint32_t            GetAddressUnitBits                () { return m_addressUnitBits        ; }
  uint32_t            GetWidth                          () { return m_width                  ; }
  uint64_t            GetResetValue                     () { return m_resetValue             ; }
  uint64_t            GetResetMask                      () { return m_resetMask              ; }
  SvdTypes::Access    GetAccess                         () { return m_access                 ; }

  bool                SetSchemaVersion                  (const std::string& schemaVersion) { m_schemaVersion = schemaVersion; return true; }
  const std::string&  GetSchemaVersion                  () { return m_schemaVersion; }

  bool                GetHasAnnonUnions                 () { return m_hasAnnonUnions;              }
  bool                SetHasAnnonUnions                 () { m_hasAnnonUnions = true; return true; }

  SvdCExpression::RegList& GetExpressionRegistersList   () { return m_expressionRegList; }

protected:

private:
  SvdCpu                           *m_cpu;
  bool                              m_hasAnnonUnions;
  uint32_t                          m_addressUnitBits;
  uint32_t                          m_width;
  uint64_t                          m_resetValue;
  uint64_t                          m_resetMask;
  SvdTypes::Access                  m_access;

  std::string                       m_schemaVersion;
  std::string                       m_fileName;
  std::string                       m_vendor;
  std::string                       m_vendorId;
  std::string                       m_series;
  std::string                       m_version;
  std::string                       m_licenseText;
  std::string                       m_headerSystemFilename;
  std::string                       m_headerDefinitionsPrefix;

  std::map<uint32_t, SvdInterrupt*> m_interruptList;
  std::list<SvdCluster*>            m_clusterList;
  std::list<SvdPeripheral*>         m_peripheralList;

  std::map<std::string, SvdItem*>                     m_perisMap;
  std::map<std::string, SvdItem*>                     m_perisHeaderStructMap;
  std::map<std::string, SvdItem*>                     m_perisHeaderEnumMap;
  std::map<uint32_t, std::list<SvdPeripheral*> >      m_perisBaseAddrMap;
  SvdCExpression::RegList                             m_expressionRegList;
};

#endif // SvdDevice_H
