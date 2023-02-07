/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdarg.h>
#include "SvdItem.h"
#include "MemoryMap.h"
#include "FileIo.h"
#include "ErrLog.h"
#include "SvdDevice.h"
#include "SvdPeripheral.h"
#include "SvdRegister.h"
#include "SvdCluster.h"
#include "SvdField.h"
#include "SvdEnum.h"
#include "SvdInterrupt.h"
#include "SvdAddressBlock.h"
#include "SvdDimension.h"
#include "SvdDerivedFrom.h"
#include "SfdGenerator.h"

#include <string>
#include <set>
#include <list>
#include <map>
using namespace std;


const string MemoryMap::m_access_str[] = {
  "UNDEF",
  "ro",
  "wo",
  "rw",
  "rw",
  "rw",
};

MemoryMap::MemoryMap(FileHeaderInfo &fileHeaderInfo) :
  m_fileIo(nullptr),
  m_gen(nullptr)
{
  m_fileIo = new FileIo();
  m_gen    = new SfdGenerator(m_fileIo);

  m_fileIo->SetSvdFileName        (fileHeaderInfo.svdFileName);
  m_fileIo->SetProgramDescription (fileHeaderInfo.descr);
  m_fileIo->SetCopyrightString    (fileHeaderInfo.copyright);
  m_fileIo->SetVersionString      (fileHeaderInfo.version);
  m_fileIo->SetBriefDescription   ("CMSIS-SVD Listing File");
}

MemoryMap::~MemoryMap()
{
  delete m_gen;
  m_fileIo->Close();
  delete m_fileIo;
}


bool MemoryMap::Interrupt(SvdDevice *device)
{
  if(!device)
    return false;

  const auto& interrupts = device->GetInterruptList();
  if(interrupts.empty())
    return true;       // no interrupts

  uint32_t idx=0;
  for(const auto& [key, interrupt] : interrupts) {
    if(!interrupt) {
      continue;
    }

    const string name  = interrupt->GetNameCalculated();
    const string descr = interrupt->GetDescriptionCalculated();
    uint32_t     num   = interrupt->GetValue();

    for(; idx<num; idx++) {
      m_fileIo->WriteLine("%03d \t\t%s", idx, "---");
    }
    idx++;
    m_fileIo->WriteLine("%03d \t\t%s \t\t\t\t\t%s", num, name.c_str(), descr.c_str());
  }

  return true;
}

bool MemoryMap::AddressBlock(SvdPeripheral *peripheral)
{
  if(!peripheral)  {
    return false;
  }

  m_fileIo->WriteLine("AddressBlock:");

  uint64_t periBaseAddr = peripheral->GetAbsoluteAddress();

  const auto& addrBlocks = peripheral->GetAddressBlock();
  if(addrBlocks.empty()) {
    return true;          // empty address blocks
  }

  uint32_t idx = 0;
  for(const auto addrBlock : addrBlocks) {
    if(!addrBlock) {
      continue;
    }

    uint32_t offs   = (uint32_t) addrBlock->GetOffset();
    uint32_t size   = (uint32_t) addrBlock->GetSize();
    uint32_t start  = (uint32_t) periBaseAddr + offs;
    uint32_t end    = start + size;

    m_fileIo->WriteLine("%i: [0x%08x ... 0x%08x] Offs: 0x%x, Size: 0x%x", idx++, end-1, start, offs, size);
  }

  return true;
}

bool MemoryMap::ClusterInfo(SvdCluster *item)
{
  string name;
  SvdDimension* dim = item->GetDimension();
  SvdExpression* expr = nullptr;
  if(dim) {
    expr = dim->GetExpression();
  }
  if(expr) {
    name    =  expr->GetName();
  } else {
    name    =  item->GetName();
  }

  uint32_t      address = (uint32_t) item->GetAbsoluteAddress();
  uint32_t      offset  = (uint32_t) item->GetOffset();
  uint32_t      bitWidth = item->GetEffectiveBitWidth() / 8;
  //const string &accType = SvdTypes::GetAccessType(item->GetAccess());
  const string &accType = m_access_str[(uint32_t)item->GetAccess()];

  m_fileIo->WriteLine("  %s \r\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t: Address: 0x%08x, \tOffset: 0x%08x, \tWidth: %i, \tAccess: %s",
    name.c_str(), address, offset, bitWidth, accType.c_str());

  return true;
}


bool MemoryMap::RegisterInfo(SvdRegister *item)
{
  string name;
  SvdDimension* dim = item->GetDimension();
  SvdExpression* expr = nullptr;
  if(dim) {
    expr = dim->GetExpression();
  }
  if(expr) {
    name =  expr->GetName();
  } else {
    name =  item->GetName();
  }

  uint32_t   address    =  (uint32_t)item->GetAbsoluteAddress();
  uint32_t   offset     =  (uint32_t)item->GetOffset();
  uint32_t   bitWidth   = item->GetEffectiveBitWidth()/8;
  //const string &accType = SvdTypes::GetAccessType(item->GetAccess());
  const string &accType = m_access_str[(uint32_t)item->GetAccess()];

  m_fileIo->WriteLine("");
  m_fileIo->WriteLine("    %s \r\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t: Address: 0x%08x, \tOffset: 0x%08x, \tWidth: %i, \tAccess: %s",
    name.c_str(), address, offset, bitWidth, accType.c_str());

  return true;
}


bool MemoryMap::FieldInfo(SvdField *item)
{
  const auto& name      = item->GetName();
  uint32_t    offset    =  (uint32_t)item->GetOffset();
  uint32_t    bitWidth  = item->GetEffectiveBitWidth();
  //const string &accType = SvdTypes::GetAccessType(item->GetAccess());
  const auto &accType   = m_access_str[(uint32_t)item->GetAccess()];

   m_fileIo->WriteLine("    %s \r\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t: [%2i ... %2i] <%s> \r\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tBits: %i",
     name.c_str(), offset+bitWidth-1, offset, accType.c_str(), bitWidth);

  return true;
}

bool MemoryMap::EnumInfo(SvdEnum *item)
{
  const auto& name  = item->GetName();
  uint32_t    value = (uint32_t)item->GetValue().u32;

   m_fileIo->WriteLine("\r\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t: %i: %s",
     value, name.c_str());

  return true;
}

bool MemoryMap::PeripheralInfo(SvdItem* item)
{
  string name;
  SvdDimension* dim = item->GetDimension();
  SvdExpression* expr = nullptr;
  if(dim) {
    expr = dim->GetExpression();
  }
  if(expr) {
    name =  expr->GetName();
  } else {
    name =  item->GetName();
  }

  //m_fileIo->WriteLine("%s", name.c_str());
  m_gen->Generate<sfd::DESCR|sfd::SUBPART>("%s", name.c_str());
  m_fileIo->WriteLine("Base Address: 0x%08x", (uint32_t) item->GetAbsoluteAddress());

  return true;
}

bool MemoryMap::DimInfo(SvdItem* item)
{
  SvdItem*      parent  = item->GetParent();
  SvdDimension* dim     = item->GetDimension();
  if(!dim) {
    SvdDimension *dimParent = dynamic_cast<SvdDimension*>(parent);
    if(dimParent) {    // item is child of <dim> Object
      parent = parent->GetParent();
      item = parent;
      dim     = item->GetDimension();
    }
  }
  if(!dim || !parent)
    return false;

  //const string &name  = item->GetName();
  //SvdExpression* expr = dim->GetExpression();

  string index = "";
  uint32_t cnt=0;
  const auto& dimList = dim->GetDimIndexList();
  for(const auto& dimItem : dimList) {
    if(!index.empty()) {
      index += ",";
    }

    if(cnt++ > 16) {
      index += "...";
      break;
    }

    index += dimItem;
  }

  //m_fileIo->WriteLine("Dimed from '%s' [%s]", name.c_str(), index.c_str());

  return true;
}

bool MemoryMap::DeriveInfo(SvdItem* item)
{
  SvdDerivedFrom *derived = item->GetDerivedFrom();
  if(!derived) {
    return false;
  }

  const auto& name = derived->GetName();

  m_fileIo->WriteLine("Derived from from '%s'", name.c_str());

  return true;
}

bool MemoryMap::EnumContainer(SvdEnumContainer* enu)
{
  string name = enu->GetName();
  if(name == "") name = "<unnamed>";
  m_fileIo->WriteLine("\r\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t%s", name.c_str());

  DeriveInfo (enu);
  DimInfo    (enu);

  return true;
}

bool MemoryMap::EnumValue(SvdEnum *enu)
{
  EnumInfo   (enu);
  DeriveInfo (enu);
  DimInfo    (enu);

  return true;
}

bool MemoryMap::Field(SvdField *field)
{
  FieldInfo  (field);
  DeriveInfo (field);
  DimInfo    (field);

  return true;
}

bool MemoryMap::Register(SvdRegister *reg)
{
  RegisterInfo  (reg);
  DeriveInfo    (reg);
  DimInfo       (reg);

  return true;
}

bool MemoryMap::Cluster(SvdCluster *cluster)
{
  ClusterInfo   (cluster);
  DeriveInfo    (cluster);
  DimInfo       (cluster);

  return true;
}

bool MemoryMap::Peripheral(SvdPeripheral *peri)
{
  PeripheralInfo  (peri);
  DeriveInfo      (peri);
  DimInfo         (peri);
  AddressBlock    (peri);

  return true;
}

bool MemoryMap::IteratePeripherals(SvdDevice *device, MapLevel mapLevel)
{
  m_gen->Generate<sfd::DESCR|sfd::PART  >("Peripheral Map");

  const auto peripheralCont = device->GetPeripheralContainer();
  if(!peripheralCont) {
    return false;
  }

  const auto& childs = peripheralCont->GetChildren();
  if(childs.empty()) {
    return false;
  }

  for(const auto child : childs) {
    const auto peri = dynamic_cast<SvdPeripheral*>(child);
    if(!peri) {
      continue;
    }

   const auto dimension = peri->GetDimension();
    if(dimension) {
      m_fileIo->WriteLine("Dim Peripheral:");
    }

    Peripheral(peri);

    m_fileIo->WriteLine("\nRegisters:");

    if(mapLevel >= MAPLEVEL_REGISTER)
      IterateRegisters(peri, mapLevel);

    if(!dimension) {
      continue;
    }

    // Dim
    const auto& dimChilds = dimension->GetChildren();
    for(const auto dimChild : dimChilds) {
      const auto dimPeri = dynamic_cast<SvdPeripheral*>(dimChild);
      if(!dimPeri) {
        continue;
      }

      Peripheral(dimPeri);

      if(mapLevel >= MAPLEVEL_REGISTER) {
        IterateRegisters(dimPeri, mapLevel);
      }
    }
  }

  return true;
}

bool MemoryMap::IterateClusterRegisters(SvdCluster *inClust, MapLevel mapLevel)
{
  const auto& childs = inClust->GetChildren();
  if(childs.empty()) {
    return true;
  }

  for(const auto child : childs) {
    const auto reg   = dynamic_cast<SvdRegister*>(child);
    const auto clust = dynamic_cast<SvdCluster*>(child);

    if(!reg && !clust) {
      continue;
    }

    SvdDimension *dim = nullptr;
    if(reg) {
      dim = reg->GetDimension();
      if(dim) {
         m_fileIo->WriteLine("Dim Register:");
      }

      Register(reg);
      if(mapLevel >= MAPLEVEL_FIELD)
        IterateFields(reg, mapLevel);
    }
    if(clust) {
      dim = clust->GetDimension();
      if(dim) {
         m_fileIo->WriteLine("Dim Cluster:");
      }

      Cluster(clust);
      IterateClusterRegisters(clust, mapLevel);
    }

    if(!dim) {
      continue;
    }

    // Dim
    const auto& dimChilds = dim->GetChildren();
    for(const auto dimChilds : dimChilds) {
      const auto dimReg   = dynamic_cast<SvdRegister*>(dimChilds);
      const auto dimClust = dynamic_cast<SvdCluster*>(dimChilds);
      if(!dimReg && !dimClust) {
        continue;
      }

      if(dimReg) {
        Register(dimReg);
        if(mapLevel >= MAPLEVEL_FIELD) {
          IterateFields(dimReg, mapLevel);
        }
      }
      if(dimClust) {
        Cluster(dimClust);
        IterateClusterRegisters(dimClust, mapLevel);
      }
    }
  }

  return true;
}

bool MemoryMap::IterateRegisters(SvdPeripheral *peri, MapLevel mapLevel)
{
  // ------------------  Register/Cluster container  ------------------
  const auto registerCont = peri->GetRegisterContainer();
  if(!registerCont) {
    return false;
  }

  const auto& childs = registerCont->GetChildren();
  if(childs.empty()) {
    return false;
  }

  for(const auto child : childs) {
    const auto reg   = dynamic_cast<SvdRegister*>(child);
    const auto clust = dynamic_cast<SvdCluster*>(child);
    if(!reg && !clust) {
      continue;
    }

    SvdDimension* dim = nullptr;
    if(reg) {
      dim = reg->GetDimension();
      if(dim) {
         ; //m_fileIo->WriteLine("Dim Register:");
      } else {
        Register(reg);
      }

      if(mapLevel >= MAPLEVEL_FIELD && !dim) {
        IterateFields(reg, mapLevel);
      }
    }
    if(clust) {
      dim = clust->GetDimension();
      if(dim) {
         m_fileIo->WriteLine("Dim Cluster:");
      }

      Cluster(clust);
      IterateClusterRegisters(clust, mapLevel);
    }

    if(!dim) {
      continue;
    }

    // Dim
    const auto& dimChilds = dim->GetChildren();
    for(const auto dimChild : dimChilds) {
      const auto dimReg   = dynamic_cast<SvdRegister*>(dimChild);
      const auto dimClust = dynamic_cast<SvdCluster*>(dimChild);
      if(!dimReg && !dimClust) {
        continue;
      }

      if(dimReg) {
        Register(dimReg);
        if(mapLevel >= MAPLEVEL_FIELD) {
          IterateFields(dimReg, mapLevel);
        }
      }
      if(dimClust) {
        Cluster(dimClust);
        IterateClusterRegisters(dimClust, mapLevel);
      }
    }
  }
  return true;
}

bool MemoryMap::IterateFields(SvdRegister *reg, MapLevel mapLevel)
{
  // ------------------  Registers  ------------------
  const auto fieldCont = reg->GetFieldContainer();
  if(!fieldCont) {
    return false;
  }

  const auto& childs = fieldCont->GetChildren();
  if(childs.empty()) {
    return false;
  }

  for(const auto child : childs) {
    const auto field = dynamic_cast<SvdField*>(child);
    if(!field) {
      continue;
    }

    SvdDimension *dim = field->GetDimension();
    if(dim) {
       ;
    }
    else {
      Field(field);
    }

    IterateEnums(field, mapLevel);

    if(!dim) {
      continue;
    }

    // Dim
    const auto& dimChilds = dim->GetChildren();
    for(const auto dimChild : dimChilds) {
      const auto dimField = dynamic_cast<SvdField*>(dimChild);
      if(!dimField) {
        continue;
      }

      Field(dimField);
      IterateEnums(dimField, mapLevel);
    }
  }
  return true;
}

// not finished, will print enum values
bool MemoryMap::IterateEnums(SvdField *field, MapLevel mapLevel)
{
  // ------------------  Registers  ------------------
  const auto& enumConts = field->GetEnumContainer();
  if(enumConts.empty()) {
    return false;
  }

  for(const auto enumCont : enumConts) {
    if(!enumCont) {
      continue;
    }

    //EnumContainer(enumCont);
#if 0
    SvdDimension *dim = enumCont->GetDimension();
    if(dim) {
        m_fileIo->WriteLine("Dim EnumContainer:");
    }
    EnumValue(enumCont);

    if(!dim) {
      continue;
    }

    // Dim
    const list<SvdItem*>& dimChilds = dim->GetChildren();
    for(const auto dimChild : dimChilds) {
      SvdEnum *dimEnu = dynamic_cast<SvdEnum*>(dimChild);
      if(!dimEnu) {
        continue;
      }

      EnumValue(dimEnu);
    }
#endif

    const auto& childs = enumCont->GetChildren();
    if(childs.empty()) {
      return false;
    }

    for(const auto child : childs) {
      const auto enu = dynamic_cast<SvdEnum*>(child);
      if(!enu) {
        continue;
      }

      SvdDimension *dim = enu->GetDimension();
      if(dim) {
         m_fileIo->WriteLine("Dim Enum:");
      }
      //EnumValue(enu);

      if(!dim) {
        continue;
      }

      // Dim
      const auto& dimChilds = dim->GetChildren();
      for(const auto dimChild : dimChilds) {
        SvdEnum *dimEnu = dynamic_cast<SvdEnum*>(dimChild);
        if(!dimEnu) {
          continue;
        }

        //EnumValue(dimEnu);
      }
    }
  }
  return true;
}

bool MemoryMap::CreateMap(SvdItem *item, const string &fileName, MapLevel mapLevel)
{
  m_fileIo->Create(fileName);

  const auto device = dynamic_cast<SvdDevice*>(item);
  if(!device) {
    return false;
  }

  //Generate(MAKE|MK_FILE, "%s %s_Peripheral.txt", devName, MEMORYMAP_FILENAME);
  const auto& devName = item->GetName();
  m_fileIo->WriteLine("%s Listing\n--------------------------------", devName.c_str());
  m_fileIo->WriteLine("Interrupts:");
  Interrupt(device);
  IteratePeripherals(device, mapLevel);

  m_fileIo->Close();
  return true;
}
