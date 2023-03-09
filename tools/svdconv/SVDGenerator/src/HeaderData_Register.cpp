/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdarg.h>
#include "HeaderData.h"
#include "HeaderGenerator.h"
#include "SvdItem.h"
#include "SvdRegister.h"
#include "SvdCluster.h"
#include "SvdField.h"
#include "SvdDimension.h"
#include "SvdTypes.h"
#include "SvdUtils.h"
#include "HeaderGenAPI.h"

using namespace std;
using namespace c_header;


bool HeaderData::CreateRegisters(SvdItem* container)
{
  REGMAP m_sortedRegs;

  AddRegisters(container, m_sortedRegs);
  CreateSortedRegisters(m_sortedRegs);

  return true;
}

bool HeaderData::AddRegisters(SvdItem* container, REGMAP &m_sortedRegs)
{
  const auto& childs = container->GetChildren();
  for(const auto item : childs) {
    if(!item || !item->IsValid()) {
      continue;
    }

    const auto dim = item->GetDimension();
    if(dim) {
      if(dim->GetExpression()->GetType() == SvdTypes::Expression::EXTEND) {
        AddRegisters(dim, m_sortedRegs);
        continue;
      }
    }

    uint64_t addr = item->GetAddress(); //GetAbsoluteOffset();
    addr &= 0xfffffffc;
    m_sortedRegs[addr].push_back(item);
  }

  return true;
}

bool HeaderData::CreateSortedRegisters(const REGMAP &regs)
{
  for(const auto& [addr, regGroup] : regs) {
    CreateSortedRegisterGroup(regGroup, addr);
  }

  return true;
}

uint32_t HeaderData::CreateSvdItem(SvdItem *item, uint64_t address)
{
  if(!item) {
    return true;
  }

  uint32_t sizeNeeded = 0;
  uint32_t addr = (uint32_t) address;

  const auto reg = dynamic_cast<SvdRegister*>(item);
  if(reg) {
    sizeNeeded = CreateRegister(reg);
  }

  const auto clust = dynamic_cast<SvdCluster*>(item);
  if(clust) {
    sizeNeeded = CreateRegCluster(clust);
  }

  uint32_t regAddress=0;
  if(item->GetParent()->GetSvdLevel() == L_Cluster) {
    regAddress = (uint32_t)item->GetAddress()        /* + localOffset*/;
  } else {
  //regAddress = (uint32_t)item->GetAbsoluteOffset() /* + localOffset*/;
    regAddress = (uint32_t)item->GetAddress()        /* + localOffset*/;
  }

  if(regAddress != addr) {
    m_gen->Generate<C_ERROR >("Address missmatch: actual: 0x%08x, should be: 0x%08x", item->GetLineNumber(), addr, regAddress);
  }

  return sizeNeeded;
}

bool HeaderData::CheckAlignment(SvdItem* item)
{
  uint32_t address = (uint32_t)item->GetAddress();
  uint32_t pos     = address & 0x03;
  uint32_t size    = item->GetEffectiveBitWidth() / 8;

  if(item->GetSvdLevel() != L_Register) {   // Cluster: just try & create ...
    return true;
  }

  const auto dim = item->GetDimension();
  if(dim) {
    if(dim->GetExpression()->GetType() == SvdTypes::Expression::ARRAY) {   // RegisterArray: just try & create ...
      return true;
    }
  }

  if(/* (pos == 0 && size > 4) \
    ||*/(pos == 1 && size > 3) \
    ||  (pos == 2 && size > 2) \
    ||  (pos == 3 && size > 1) )
  {
        const auto& name = item->GetName();
        m_gen->Generate<C_ERROR >("Unaligned Registers are not supported: '%s' addr: 0x%08x pos: %d, size: %d", item->GetLineNumber(), name.c_str(), address, pos, size);
        return false;
  }

  return true;
}

bool HeaderData::CreateSortedRegisterGroup(const list<SvdItem*>& regGroup, uint64_t baseAddr)
{
  RegSorter regSorter;
  memset(&regSorter, 0, sizeof(RegSorter));

  for(const auto item : regGroup) {
    if(!item) {
      continue;
    }

    if(!CheckAlignment(item)) {
      continue;   // do not use unaligned registers
    }

    AddNode_Register(item, &regSorter);
  }

  regSorter.address = (uint32_t)baseAddr;
  GeneratePart(&regSorter);

  return true;
}

uint32_t HeaderData::CreateRegister(SvdRegister* reg)
{
  const auto name        = reg->GetHeaderFileName();
  const auto dataTypeStr = reg->GetHeaderTypeName();
  const auto descr       = reg->GetDescriptionCalculated();
  uint32_t   addr        = (uint32_t) reg->GetAddress();
  const auto accessType  = reg->GetEffectiveAccess();
  uint32_t   size        = reg->GetEffectiveBitWidth() / 8;
  const auto dim         = reg->GetDimension();
  bool bFieldsAsStruct   = m_options.IsCreateFields();
  bool bAnsiCStruct      = m_options.IsCreateFieldsAnsiC();
  bool bHasFields        = reg->HasValidFields();
  const auto fields      = reg->GetFieldContainer();

  if(fields) {
    bHasFields &= fields->GetChildCount()? true : false;
  }
  else {
    bHasFields = false;
  }

  bool generateFields = (bFieldsAsStruct && bHasFields)? true : false;

  string regName;
  string unionName;
  string structName;

  if(generateFields && bAnsiCStruct) {
    regName     = "reg";
    unionName   = name;
    structName  = "bit";
  }
  else {
    regName     = name;
    unionName   = "";
    structName  = name;
    structName += "_b";
  }

  if(dim) {
    uint32_t num = dim->GetDim();

    regName += "[";
    regName += SvdUtils::CreateDecNum(num);
    regName += "]";

    structName += "[";
    structName += SvdUtils::CreateDecNum(num);
    structName += "]";

    size *= num;
  }

  if(generateFields) {
    m_gen->Generate<UNION|BEGIN >("");
  }

  m_gen->Generate<MAKE|MK_REGISTER_STRUCT    >("%s", accessType, dataTypeStr.c_str(), size, regName.c_str());
  m_gen->Generate<MAKE|MK_DOXY_COMMENT_ADDR  >("%s", addr, !descr.empty()? descr.c_str() : name.c_str());

  if(generateFields) {
    CreateFields(reg, structName);
    m_gen->Generate<UNION|END                >("%s", unionName.c_str());
  }

  return size;
}

uint32_t HeaderData::CreateRegCluster(SvdCluster*  cluster)
{
  const auto headerTypeName     = cluster->GetHeaderTypeNameHierarchical();//GetHeaderTypeName();
  const auto regName            = cluster->GetNameCalculated();
  const auto descr              = cluster->GetDescriptionCalculated();
  uint32_t   addr               = (uint32_t) cluster->GetAddress();
  SvdTypes::Access accessType   = cluster->GetEffectiveAccess();
  uint32_t   size               = cluster->GetSize();

  string name = headerTypeName;
  name += "_Type";

  const auto dim = cluster->GetDimension();
  if(dim) {
    uint32_t num = dim->GetDim();
    m_gen->Generate<MAKE|MK_REGISTER_STRUCT    >("%s[%d]", accessType, name.c_str(), size, regName.c_str(), num);
    size *= num;
  } else {
    m_gen->Generate<MAKE|MK_REGISTER_STRUCT    >("%s",     accessType, name.c_str(), size, regName.c_str());
  }

  m_gen->Generate<MAKE|MK_DOXY_COMMENT_ADDR  >("%s", addr, !descr.empty()? descr.c_str() : name.c_str());

  return size;
}
