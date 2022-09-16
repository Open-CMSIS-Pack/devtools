/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdarg.h>
#include "SfdData.h"
#include "SfdGenerator.h"
#include "SvdItem.h"
#include "ErrLog.h"
#include "SvdDevice.h"
#include "SvdPeripheral.h"
#include "SvdRegister.h"
#include "SvdCluster.h"
#include "SvdField.h"
#include "SvdEnum.h"
#include "SvdCpu.h"
#include "SvdInterrupt.h"
#include "SvdDimension.h"
#include "SvdTypes.h"
#include "SvdUtils.h"
#include "SfdGenAPI.h"

using namespace std;
using namespace sfd;


const string SfdData::m_addrWidthStr[] = {
  "char",
  "short",
  "int",
  "int",
  "int",
  "int",
  "int",
  "int",
};



bool SfdData::CreateDevice(SvdDevice* device)
{
  CreateExpressionRefs(device);

  list<SvdItem*> peripheralList;

  const auto cont = device->GetPeripheralContainer();
  if(cont && cont->GetChildCount()) {
    CreatePeripherals(cont, peripheralList);
  }

  CreateInterruptItems(device);
  CreatePeripheralMenu(device, peripheralList);

  return true;
}

bool SfdData::CreateExpressionRefs(SvdDevice* device)
{
  if(!device) {
    return true;
  }

  const auto& regList = device->GetExpressionRegistersList();
  if(regList.empty()) {
    return true;
  }

  for(const auto& [regName, regItem] : regList) {
    const auto reg = dynamic_cast<SvdRegister*>(regItem);
    if(!reg) {
      continue;
    }

    const auto  itemName   = reg->GetHierarchicalName();
    const auto  dispName   = reg->GetDisplayNameCalculated();
    const auto  descr      = SvdUtils::CheckTextGeneric_SfrCC2(reg->GetDescriptionCalculated());
    uint32_t    address    = (uint32_t)reg->GetAbsoluteAddress();
    uint32_t    regWidth   = reg->GetEffectiveBitWidth();

    if(regWidth < 8) {
      regWidth = 8;
    }

    const auto& strAddrWidth = m_addrWidthStr[(regWidth/8)-1];
    reg->SetUsedForExpression();   // mark as already generated

    CreateItemDescription(reg, "Expression Object");
    m_gen->Generate<MAKE|MK_ADDRSTR>(ADDRESS_STRING, strAddrWidth.c_str(), itemName.c_str(), address);
  }

  return true;
}

bool SfdData::CreateInterruptItems(SvdDevice* device)
{
  if(!device) {
    return false;
  }
  if(device->GetInterruptList().empty()) {
    return true;
  }

  list<string> interruptList;

  CreateItemDescription(device, "IRQ Num definition");
  m_gen->Generate<DESCR|PART >("Interrupt Number Definition");

  const auto cpu = device->GetCpu();
  int32_t nvicPrioBits = -1;

  // CPU specific
  if(cpu) {
    const auto cpuType = cpu->GetType();
    const auto& name = SvdTypes::GetCpuName(cpuType);
    m_gen->Generate<DESCR|SUBPART>("%s Specific Interrupt Numbers", name.c_str());

    const auto& interrupts = cpu->GetInterruptList();
    for(const auto& [key, interrupt] : interrupts) {
      if(!IsValid(interrupt)) {
        continue;
      }

      const auto name  = interrupt->GetNameCalculated();
      const auto descr = SvdUtils::CheckTextGeneric_SfrCC2(interrupt->GetDescriptionCalculated());
      uint32_t   num   = interrupt->GetValue();

      m_gen->Generate<MAKE|MK_INTERRUPT>("%s", num, descr.c_str(), name.c_str());
      interruptList.push_back(name);
    }
    nvicPrioBits = cpu->GetNvicPrioBits();
  }

  // Device specific
  if(device) {
    const auto& name = device->GetName();
    m_gen->Generate<DESCR|SUBPART>("%s Specific Interrupt Numbers", name.c_str());

    const auto& interrupts = device->GetInterruptList();
    for(const auto& [key, interrupt] : interrupts) {
      if(!IsValid(interrupt)) {
        continue;
      }

      const auto name  = interrupt->GetNameCalculated();
      const auto descr = SvdUtils::CheckTextGeneric_SfrCC2(interrupt->GetDescriptionCalculated());
      uint32_t   num   = interrupt->GetValue();

      m_gen->Generate<MAKE|MK_INTERRUPT>("%s", num+16, descr.c_str(), name.c_str());
      interruptList.push_back(name);
    }
  }

  if(interruptList.size()) {
    const auto& devName = device->GetName();

    m_gen->Generate<IRQTABLE|BEGIN >("%s_IRQTable", devName.c_str());
    m_gen->Generate<NAME          >("%s Interrupt Table", devName.c_str());

    if(nvicPrioBits != -1) {
      m_gen->Generate<NVICPRIOBITS   >("%i", nvicPrioBits);
    }

    for(const auto& name : interruptList) {
      m_gen->Generate<QITEM       >("%s_IRQ",   name.c_str());
    }

    m_gen->Generate<IRQTABLE|END    >("");
  }

  return true;
}

bool SfdData::CreatePeripheralMenu(SvdDevice* device, list<SvdItem*>& list)
{
  map<string, map<string, SvdPeripheral*, SvdUtils::StringAlnumLess> > menuMap;

  CreateItemDescription(device, "Menu");
  const auto& instanceName = device->GetName();
  m_gen->Generate<DESCR|SUBPART >("Peripheral Menu: '%s'", instanceName.c_str());

  for(const auto item : list) {
    const auto peri = dynamic_cast<SvdPeripheral*>(item);
    if(!IsValid(peri)) {
      continue;
    }

    const auto periName = peri->GetNameCalculated();
    if(periName.empty()) {
      continue;
    }

    const auto& periGroupName = peri->GetGroupName();
    string groupName;
    if(!periGroupName.empty()) {
      groupName = periGroupName;
    }
    else {
      groupName = SvdUtils::FindGroupName(periName);
    }

    menuMap[groupName][periName] = peri;
  }

  m_gen->Generate<DESCR|PART >("Main Menu");

  for(const auto& [menuName, subMenu] : menuMap) {
    m_gen->Generate<BLOCK|BEGIN >("%s", menuName.c_str());

    for(const auto& [subMenuName, value] : subMenu) {
      m_gen->Generate<MEMBER >("%s", subMenuName.c_str());
    }

    m_gen->Generate<ENDGROUP >("");
  }

  return true;
}

bool SfdData::CreateDisableCondition(SvdPeripheral* peri)
{
  const auto disableCondition = peri->GetDisableCondition();
  if(!disableCondition || !disableCondition->IsValid()) {
    return true;
  }

  const auto disableCond = disableCondition->GetExpressionString();
  if(disableCond.empty()) {
    return false;
  }

  m_gen->Generate<DISABLECOND >("%s", disableCond.c_str());

  return true;
}

bool SfdData::CreatePeripheralView(SvdPeripheral* peri, list<SvdItem*> registerList)
{
  const auto periName       = peri->GetNameCalculated();
  const auto hierarchicalName = peri->GetHierarchicalName();
  const auto displayName    = peri->GetDisplayNameCalculated();

  CreateItemDescription(peri, "View");
  m_gen->Generate<VIEW|BEGIN>("%s",    periName.c_str());
  CreateDisableCondition(peri);
  m_gen->Generate<NAME>("%s",    displayName.c_str());
  CreateItemList(registerList);
  m_gen->Generate<ENDGROUP >("");

  return true;
}

bool SfdData::CreatePeripheralArrayItree(const string& prefix, SvdPeripheral* peri, list<SvdItem*>& periArrayList)
{
  if(!peri) {
    return false;
  }

  const auto name     = peri->GetHierarchicalName();
  const auto dispName = peri->GetDisplayNameCalculated();
  const auto descr    = SvdUtils::CheckTextGeneric_SfrCC2(peri->GetDescriptionCalculated());

  CreateItemDescription(peri, "Array ITree");
  m_gen->Generate<ITREE|BEGIN >("SFDITEM_PERI__%s", name.c_str());

  const auto dim = dynamic_cast<SvdDimension*>(peri->GetParent());
  if(dim && dim->GetExpression()->GetType() == SvdTypes::Expression::ARRAY) {
    const auto idx = peri->GetDimElementIndex();
    m_gen->Generate<NAME>("[%d]", idx);
  } else {
    m_gen->Generate<NAME>("%s", dispName.c_str());
  }

  m_gen->Generate<INFO >("%s", descr.c_str());
  CreateItemList(periArrayList);
  m_gen->Generate<ENDGROUP>("");

  return true;
}

bool SfdData::CreatePeripheralArrayView(const string& prefix, SvdPeripheral* peri)
{
  const auto name     = peri->GetHierarchicalName();
  const auto dispName = peri->GetDisplayNameCalculated();
  const auto descr    = SvdUtils::CheckTextGeneric_SfrCC2(peri->GetDescriptionCalculated());

  CreateItemDescription(peri, "Array View");
  m_gen->Generate<VIEW|BEGIN>("%s", name.c_str());
  m_gen->Generate<NAME>("%s", dispName.c_str());

  m_gen->Generate<ITEM >("SFDITEM_PERI__%s", name.c_str());
  m_gen->Generate<ENDGROUP>("");

  return true;
}

bool SfdData::CreatePeripherals(SvdItem* peris, list<SvdItem*>& peripheralList)
{
  const auto& childs = peris->GetChildren();
  if(childs.empty()) {
    return true;
  }

  for(const auto child : childs) {
    const auto peri = dynamic_cast<SvdPeripheral*>(child);
    if(!IsValid(peri)) {
      continue;
    }

    if(!peri->GetChildCount()) {
      continue;
    }

    CreatePeripheral(peri, peripheralList);
  }

  return true;
}

bool SfdData::CreatePeripheralArrayPeri(SvdPeripheral* peri, list<SvdItem*>& peripheralList)
{
  list<SvdItem*> registerList;

  const auto cont = peri->GetRegisterContainer();
  if(cont && cont->GetChildCount()) {
    CreateRegisters(cont, registerList);
  }

  if(!registerList.empty()) {
    CreatePeripheralArrayItree("SFDITEM_PERI__", peri, registerList);
    peripheralList.push_back(peri);
  }

  return true;
}

bool SfdData::CreatePeripheral(SvdPeripheral* peri, list<SvdItem*> &peripheralList)
{
  list<SvdItem*> registerList;

  const auto dim = peri->GetDimension();
  if(dim) {
    if(dim->GetExpression()->GetType() == SvdTypes::Expression::ARRAY) {
      list<SvdItem*> periArrayList;
      CreatePeripheralArray(peri, peripheralList);
      return true;
    }
    else {
      CreatePeripherals(dim, peripheralList);
      return true;
    }
  }

  const auto cont = peri->GetRegisterContainer();
  if(cont && cont->GetChildCount()) {
    CreateRegisters(cont, registerList);
  }

  if(!registerList.empty()) {
    CreatePeripheralView(peri, registerList);
    peripheralList.push_back(peri);
  }

  return true;
}

bool SfdData::CreatePeripheralArray(SvdPeripheral* peri, list<SvdItem*> &peripheralList)
{
  if(!peri) {
    return false;
  }

  list<SvdItem*> periArrayList;

  const auto dim = peri->GetDimension();
  if(dim) {
    const auto& childs = dim->GetChildren();
    for(const auto child : childs) {
      const auto dimPeri = dynamic_cast<SvdPeripheral*>(child);
      if(!IsValid(dimPeri)) {
        continue;
      }

      CreatePeripheralArrayPeri(dimPeri, periArrayList);
    }

    if(!periArrayList.empty()) {
      CreatePeripheralArrayItree("SFDITEM_PERI__", peri, periArrayList);
      CreatePeripheralArrayView("", peri);
      peripheralList.push_back(peri);
    }
    return true;
  }

  return true;
}

bool SfdData::CreateRegisters(SvdItem* regs, list<SvdItem*> &registerList)
{
  const auto& childs = regs->GetChildren();
  if(childs.empty()) {
    return true;
  }

  for(const auto item : childs) {
    if(!IsValid(item)) {
      continue;
    }

    if(item->GetReadAction() != SvdTypes::ReadAction::UNDEF) {
      continue;
    }

    CreateRegClust(item, registerList);
  }

  return true;
}

bool SfdData::CreateRegClust(SvdItem* item, list<SvdItem*>& registerList)
{
  if(!item) {
    return true;
  }

  const auto reg = dynamic_cast<SvdRegister*>(item);
  if(reg) {
    CreateRegister(reg, registerList);
  }

  const auto clust = dynamic_cast<SvdCluster*>(item);
  if(clust) {
    CreateCluster(clust, registerList);
  }

  return true;
}

bool SfdData::CreateRegister(SvdRegister* reg, list<SvdItem*> &registerList)
{
  if(!reg) {
    return true;
  }

  const auto dim = reg->GetDimension();
  if(dim) {
    if(dim->GetExpression()->GetType() == SvdTypes::Expression::ARRAY) {
      list<SvdItem*> regArrayList;
      CreateRegisterArray(reg, regArrayList);
      if(!regArrayList.empty()) {
        registerList.push_back(reg);
      }
      return true;
    }
    else {
      CreateRegisters(dim, registerList);
      return true;
    }
  }

  CreateRegisterItem(reg);
  registerList.push_back(reg);

  return true;
}

bool SfdData::CreateRegisterArray(SvdRegister* reg, list<SvdItem*>& registerList)
{
  const auto dim = reg->GetDimension();
  if(dim) {
    list<SvdItem*> regArrayList;

    const auto& childs = dim->GetChildren();
    for(const auto child : childs) {
      const auto dimReg = dynamic_cast<SvdRegister*>(child);
      if(!IsValid(dimReg)) {
        continue;
      }

      CreateRegisterArrayItem(dimReg);
      regArrayList.push_back(dimReg);
    }

    CreateRegisterArrayItree(reg, regArrayList);
    if(!regArrayList.empty()) {
      registerList.push_back(reg);
    }

    return true;
  }

  return true;
}

bool SfdData::CreateCluster(SvdCluster* clust, list<SvdItem*> &registerList)
{
  if(!clust) {
    return true;
  }

  list<SvdItem*> clusterList;

  const auto dim = clust->GetDimension();
  if(dim) {
    if(dim->GetExpression()->GetType() == SvdTypes::Expression::ARRAY) {
      list<SvdItem*> clustArrayList;
      CreateClusterArray(clust, clustArrayList);
      if(!clustArrayList.empty()) {
        registerList.push_back(clust);
      }
      return true;
    }
    else {
      CreateRegisters(dim, registerList);
      return true;
    }
  }

  CreateRegisters(clust, clusterList);
  if(!clusterList.empty()) {
    CreateClusterItree(clust, clusterList);
    registerList.push_back(clust);
  }

  return true;
}

bool SfdData::CreateClusterArray(SvdCluster* clust, list<SvdItem*> &registerList)
{
  if(!clust) {
    return false;
  }

  const auto dim = clust->GetDimension();
  if(dim) {
    list<SvdItem*> clustArrayList;
    const auto& childs = dim->GetChildren();
    for(const auto child : childs) {
      const auto dimClust = dynamic_cast<SvdCluster*>(child);
      if(!IsValid(dimClust)) {
        continue;
      }

      CreateCluster(dimClust, clustArrayList);
    }

    if(!clustArrayList.empty()) {
      CreateClusterArrayItree(clust, clustArrayList);
      registerList.push_back(clust);
    }
  }

  return true;
}

bool SfdData::CreateFields(SvdItem* fields, list<SvdItem*> &fieldList)
{
  if(!fields) {
    return false;
  }

  const auto& childs = fields->GetChildren();
  if(childs.empty()) {
    return true;
  }

  for(const auto child : childs) {
    const auto item = dynamic_cast<SvdField*>(child);
    if(!IsValid(item)) {
      continue;
    }

    const auto dim = item->GetDimension();
    if(dim) {
      CreateFields(dim, fieldList);
      return true;
    }

    CreateField(item, fieldList);
  }

  return true;
}

bool SfdData::CreateField(SvdField* field, list<SvdItem*> &fieldList)
{
  if(!field) {
    return true;
  }

  CreateFieldItem(field);
  fieldList.push_back(field);

  return true;
}

bool SfdData::CreateItemList (const list<SvdItem*> &list)
{
  for(const auto item : list) {
    if(!IsValid(item)) {
      continue;
    }

    const auto peri  = dynamic_cast<SvdPeripheral*> (item);
    const auto clust = dynamic_cast<SvdCluster*>    (item);
    const auto reg   = dynamic_cast<SvdRegister*>   (item);
    const auto field = dynamic_cast<SvdField*>      (item);

    string pre;
    if(peri) {
      pre = "SFDITEM_PERI__";
    }
    else if(clust) {
      pre = "SFDITEM_CLUST__";
    }
    else if(reg) {
      pre = "SFDITEM_REG__";
    }
    else if(field) {
      pre = "SFDITEM_FIELD__";
    }

    const auto name = item->GetHierarchicalName();
    m_gen->Generate<ITEM>("%s%s", pre.c_str(), name.c_str());
  }

  return true;
}

bool SfdData::CreateItemDescription_(SvdItem* item, const string& text, const string srcFile, uint32_t srcLine)
{
  if(!item) {
    return false;
  }

  const auto lineNo = item->GetLineNumber();

  const auto peri  = dynamic_cast<SvdPeripheral*> (item);
  const auto clust = dynamic_cast<SvdCluster*>    (item);
  const auto reg   = dynamic_cast<SvdRegister*>   (item);
  const auto field = dynamic_cast<SvdField*>      (item);

  string type;
  if(peri) {
    type = "Peripheral";
  }
  else if(clust) {
    type = "Cluster";
  }
  else if(reg) {
    type = "Register";
  }
  else if(field) {
    type = "Field";
  }

  const auto name = item->GetHierarchicalName();
  m_gen->Generate<DESCR_LINENO|SUBPART >("%s %s: %s", lineNo, type.c_str(), text.c_str(), name.c_str());

  if(m_options.IsDebugSfd()) {
    string dbgTxt = "Dbg: ";
    dbgTxt += srcFile;

    const auto pos = dbgTxt.find_last_of("\\");
    if(pos != string::npos) {
      dbgTxt.erase(0, pos+1);
    }

    dbgTxt += " : ";
    dbgTxt += to_string(srcLine);
    m_gen->Generate<DIRECT >("// %s", dbgTxt.c_str());
  }

  return true;
}

bool SfdData::CreateRegisterArrayItree(SvdRegister* reg, list<SvdItem*>& regArrayList)
{
  const auto itemName = reg->GetHierarchicalName();
  const auto dispName = reg->GetDisplayNameCalculated();
  const auto descr    = SvdUtils::CheckTextGeneric_SfrCC2(reg->GetDescriptionCalculated());

  string name = "SFDITEM_REG__";
  name += itemName;

  CreateItemDescription(reg, "Array ITree");
  m_gen->Generate<ITREE|BEGIN          >("%s", name.c_str());
  m_gen->Generate<NAME                 >("%s", dispName.c_str());
  m_gen->Generate<INFO                 >("%s", descr.c_str());

  CreateItemList(regArrayList);

  m_gen->Generate<ENDGROUP             >("");

  return true;
}

bool SfdData::CreateClusterArrayItree(SvdCluster* clust, list<SvdItem*> &clustArrayList)
{
  const auto itemName = clust->GetHierarchicalName();
  const auto dispName = clust->GetDisplayNameCalculated();
  const auto descr    = SvdUtils::CheckTextGeneric_SfrCC2(clust->GetDescriptionCalculated());

  string name = "SFDITEM_CLUST__";
  name += itemName;

  CreateItemDescription(clust, "Array ITree");
  m_gen->Generate<ITREE|BEGIN          >("%s", name.c_str());
  m_gen->Generate<NAME                 >("%s", dispName.c_str());
  m_gen->Generate<INFO                 >("%s", descr.c_str());

  CreateItemList(clustArrayList);
  m_gen->Generate<ENDGROUP              >("");

  return true;
}

bool SfdData::CreateClusterItree(SvdCluster* clust, list<SvdItem*> &clusterList)
{
  const auto itemName = clust->GetHierarchicalName();
  const auto dispName = clust->GetDisplayNameCalculated();
  const auto descr    = SvdUtils::CheckTextGeneric_SfrCC2(clust->GetDescriptionCalculated());

  CreateItemDescription(clust, "ITree");
  m_gen->Generate<ITREE|BEGIN          >("SFDITEM_CLUST__%s", itemName.c_str());

  const auto dim = dynamic_cast<SvdDimension*>(clust->GetParent());
  if(dim && dim->GetExpression()->GetType() == SvdTypes::Expression::ARRAY) {
    const auto idx = clust->GetDimElementIndex();
    m_gen->Generate<NAME               >("[%d]", idx);
  } else {
    m_gen->Generate<NAME               >("%s", dispName.c_str());
  }

  m_gen->Generate<INFO                 >("%s", descr.c_str());
  CreateItemList(clusterList);
  m_gen->Generate<ENDGROUP             >("");

  return true;
}

bool SfdData::CreateRegisterItem(SvdRegister* reg)
{
  const auto itemName   = reg->GetHierarchicalName();
  const auto dispName   = reg->GetDisplayNameCalculated();
  const auto descr      = SvdUtils::CheckTextGeneric_SfrCC2(reg->GetDescriptionCalculated());
  const auto address    = (uint32_t)reg->GetAbsoluteAddress();
  const auto accessType = reg->GetEffectiveAccess();
        auto regWidth   = reg->GetEffectiveBitWidth();
  const auto accRead    = (uint32_t)reg->GetAccessMaskRead();
  const auto accWrite   = (uint32_t)reg->GetAccessMaskWrite();

  if(regWidth < 8) {
    regWidth = 8;
  }

  CreateItemDescription(reg, "Item Address");
  const auto& strAddrWidth = m_addrWidthStr[(regWidth/8)-1];
  if(!reg->IsUsedForCExpression()) {
    m_gen->Generate<MAKE|MK_ADDRSTR >(ADDRESS_STRING, strAddrWidth.c_str(), itemName.c_str(), address);
  }
  else {
    m_gen->Generate<DIRECT>("// %s already generated for use in C Expressions", itemName.c_str());
  }

  const auto cont = reg->GetFieldContainer();
  if(cont && cont->IsValid() && cont->GetChildCount()) {
    list<SvdItem*> fieldsList;
    CreateFields(cont, fieldsList);
    const auto accMaskReadRtree = (uint32_t) reg->GetAccessMask();

    CreateItemDescription(reg, "RTree");
    m_gen->Generate<RTREE|BEGIN            >("SFDITEM_REG__%s", itemName.c_str());
    m_gen->Generate<NAME                   >("%s", dispName.c_str());

    switch(accessType) {
      case SvdTypes::Access::READONLY:
        m_gen->Generate<ACC_R|SINGLE       >("");
        break;
      case SvdTypes::Access::WRITEONLY:
        m_gen->Generate<ACC_W|SINGLE       >("");
        break;
      case SvdTypes::Access::READWRITE:
      case SvdTypes::Access::WRITEONCE:
      case SvdTypes::Access::READWRITEONCE:
      default:
        m_gen->Generate<ACC_RW|SINGLE      >("");
        break;
    }

    m_gen->Generate<INFO|IRANGE_ADDR_ACC   >("%s", regWidth-1, 0, address, accessType, descr.c_str());
    m_gen->Generate<MAKE|MK_EDITLOC        >("%s", 0, regWidth-1, accMaskReadRtree, accWrite, itemName.c_str());
    CreateItemList(fieldsList);
  }
  else {
    CreateItemDescription(reg, "Item");
    m_gen->Generate<ITEM|BEGIN             >("SFDITEM_REG__%s", itemName.c_str());
    m_gen->Generate<NAME                   >("%s", dispName.c_str());
    m_gen->Generate<INFO|IRANGE_ADDR_ACC   >("%s", regWidth-1, 0, address, accessType, descr.c_str());
    m_gen->Generate<EDIT|BEGIN             >("");
    m_gen->Generate<MAKE|MK_EDITLOC        >("%s", 0, regWidth-1, accRead, accWrite, itemName.c_str());
  }
  m_gen->Generate<ENDGROUP                 >("");

  return true;
}

bool SfdData::CreateRegisterArrayItem(SvdRegister* reg)
{
  const auto itemName   = reg->GetHierarchicalName();
  const auto dispName   = reg->GetDisplayNameCalculated();
  const auto descr      = SvdUtils::CheckTextGeneric_SfrCC2(reg->GetDescriptionCalculated());
  const auto address    = (uint32_t)reg->GetAbsoluteAddress();
  const auto accessType = reg->GetEffectiveAccess();
        auto regWidth   = reg->GetEffectiveBitWidth();
  const auto accRead    = (uint32_t)reg->GetAccessMaskRead();
  const auto accWrite   = (uint32_t)reg->GetAccessMaskWrite();

  if(regWidth < 8) {
    regWidth = 8;
  }

  CreateItemDescription(reg, "Array Item Address");
  const auto& strAddrWidth = m_addrWidthStr[(regWidth/8)-1];
  m_gen->Generate<MAKE|MK_ADDRSTR          >(ADDRESS_STRING, strAddrWidth.c_str(), itemName.c_str(), address);

  const auto cont = reg->GetFieldContainer();
  if(cont && reg->HasValidFields() && cont->GetChildCount()) {
    list<SvdItem*> fieldsList;

    CreateFields(cont, fieldsList);
    CreateItemDescription(reg, "Array RTree");
    m_gen->Generate<RTREE|BEGIN            >("SFDITEM_REG__%s", itemName.c_str());
    m_gen->Generate<NAME                   >("[%d]", reg->GetDimElementIndex());

    switch(accessType) {
      case SvdTypes::Access::READONLY:
        m_gen->Generate<ACC_R|SINGLE      >("");
        break;
      case SvdTypes::Access::WRITEONLY:
        m_gen->Generate<ACC_W|SINGLE      >("");
        break;
      case SvdTypes::Access::READWRITE:
      case SvdTypes::Access::WRITEONCE:
      case SvdTypes::Access::READWRITEONCE:
      default:
        m_gen->Generate<ACC_RW|SINGLE      >("");
        break;
    }

    m_gen->Generate<INFO|IRANGE_ADDR_ACC   >("%s", regWidth-1, 0, address, accessType, descr.c_str());
    m_gen->Generate<MAKE|MK_EDITLOC        >("%s", 0, regWidth-1, accRead, accWrite, itemName.c_str());
    CreateItemList(fieldsList);
  }
  else {
    CreateItemDescription(reg, "Array Item");
    m_gen->Generate<ITEM|BEGIN             >("SFDITEM_REG__%s", itemName.c_str());
    m_gen->Generate<NAME                   >("[%d]", reg->GetDimElementIndex());
    m_gen->Generate<INFO|IRANGE_ADDR_ACC   >("%s", regWidth-1, 0, address, accessType, descr.c_str());
    m_gen->Generate<EDIT|BEGIN             >("");
    m_gen->Generate<MAKE|MK_EDITLOC        >("%s", 0, regWidth-1, accRead, accWrite, itemName.c_str());
  }
  m_gen->Generate<ENDGROUP                 >("");

  return true;
}

bool SfdData::CreateFieldItem(SvdField* field)
{
  const auto fieldName  = field->GetNameCalculated();
  const auto itemName   = field->GetHierarchicalName();
  const auto dispName   = field->GetDisplayNameCalculated();
  const auto fieldDescr = SvdUtils::CheckTextGeneric_SfrCC2(field->GetDescriptionCalculated());
  const auto  regName   = field->GetParentRegisterNameHierarchical();
  const auto firstBit   = (uint32_t)field->GetOffset();
  const auto bitWidth   = (uint32_t)field->GetEffectiveBitWidth();
  const auto accessType = field->GetEffectiveAccess();
  const auto regAddress = (uint32_t)field->GetAbsoluteAddress();

  uint32_t regBitWidth = 32;
  const auto parent = field->GetParent();
  if(parent) {
    const auto fCont = dynamic_cast<SvdFieldContainer*>(field->GetParent());
    if(fCont) {
      const auto reg = dynamic_cast<SvdRegister*>(fCont->GetParent());
      if(reg) {
        regBitWidth = reg->GetEffectiveBitWidth();
      }
    }
  }

  string valuesDescr;
  field->GetValuesDescriptionString(valuesDescr);
  valuesDescr = SvdUtils::CheckTextGeneric_SfrCC2(valuesDescr);

  uint32_t lastBit = firstBit + bitWidth -1;
  uint32_t bitMaxNum = (uint32_t) (((uint64_t)(1) << bitWidth) -1);

  CreateItemDescription(field, "Item");
  m_gen->Generate<ITEM|BEGIN>("SFDITEM_FIELD__%s", itemName.c_str());
  m_gen->Generate<NAME      >("%s", dispName.c_str());

  uint32_t accRead, accWrite;
  switch(accessType) {
    case SvdTypes::Access::READONLY:
      accRead  = bitMaxNum;
      accWrite = 0;
      m_gen->Generate<ACC_R|SINGLE>("");
      break;
    case SvdTypes::Access::WRITEONLY:
      accRead  = 0;
      accWrite = bitMaxNum;
      m_gen->Generate<ACC_W|SINGLE>("");
      break;
    case SvdTypes::Access::READWRITE:
    case SvdTypes::Access::WRITEONCE:
    case SvdTypes::Access::READWRITEONCE:
    default:
      accRead  = bitMaxNum;
      accWrite = bitMaxNum;
      m_gen->Generate<ACC_RW|SINGLE>("");
      break;
  }

  string descr;
  if(!valuesDescr.empty()) {
    descr += "\\n";
  }
  descr += fieldDescr;
  if(!valuesDescr.empty()) {
    descr += "\\n";
    descr += valuesDescr;
  }

  if(bitWidth < 2) {
    m_gen->Generate<INFO|IBIT_ADDR_ACC>("%s",             firstBit, regAddress, accessType, descr.c_str());
  }
  else {
    m_gen->Generate<INFO|IRANGE_ADDR_ACC>("%s", lastBit,  firstBit, regAddress, accessType, descr.c_str());
  }

  const auto& conts = field->GetEnumContainer();
  bool firstContHasChilds = false;
  if(!conts.empty()) {
    const auto cont = *conts.begin();
    if(cont) {
      if(cont->GetChildCount()) {
        firstContHasChilds = true;
      }
    }
  }

  if(bitWidth > MAX_BITWIDTH_FOR_COMBO && firstContHasChilds) {
    string msg = "Enumerated values list for field '";
    msg += field->GetNameCalculated();
    msg += "' exceeds maximum of 256 elements. List will not be generated.";
    LogMsg("M227", MSG(msg), field->GetLineNumber());
  }

  if(bitWidth <= MAX_BITWIDTH_FOR_COMBO && firstContHasChilds) {
    m_gen->Generate<COMBO|BEGIN>("");
    m_gen->Generate<MAKE|MK_OBITLOC>("%s", regBitWidth, regName.c_str());

    if(bitWidth < 2) {
      m_gen->Generate<OBIT>("%s", firstBit, dispName.c_str());
    }
    else {
      m_gen->Generate<ORANGE>("%s", lastBit, firstBit, dispName.c_str());
    }

    CreateEnumValues(conts, field);
    m_gen->Generate<COMBO|END>("");
  }
  else if(bitWidth < 2) {
    m_gen->Generate<CHECK|BEGIN     >("");
    m_gen->Generate<MAKE|MK_OBITLOC >("%s", regBitWidth, regName.c_str());
    m_gen->Generate<OBIT            >("%s", firstBit, dispName.c_str());
  } 
  else {
    m_gen->Generate<EDIT|BEGIN      >("");
    m_gen->Generate<MAKE|MK_EDITLOC >("%s", firstBit, lastBit, accRead, accWrite, regName.c_str());
  }

  m_gen->Generate<ENDGROUP>("");

  return true;
}

bool SfdData::CreateEnumValues(const list<SvdEnumContainer*>& conts, SvdField* field)
{
  if(conts.size() == 1) {
    CreateEnumValueSet(*conts.begin(), field);
  }
  else {
   // Decide, which ENUM Container to use (read, write, ...)
    CreateEnumValueSet(*conts.begin(), field);
  }

  return true;
}

bool SfdData::CreateEnumValueSet(SvdEnumContainer *cont, SvdField* field)
{
  const auto& childs = cont->GetChildren();
  if(childs.empty()) {
    return true;
  }

  const auto defaultValue = cont->GetDefaultValue();
  map<uint32_t, SvdEnum*> enumValues;

  for(const auto child : childs) {
    const auto enu = dynamic_cast<SvdEnum*>(child);
    if(!IsValid(enu)) {
      continue;
    }

    const auto val = enu->GetValue();
    if(!val.bValid) {
      continue;
    }

    enumValues[val.u32] = enu;
  }

  const auto bitWidth = field->GetEffectiveBitWidth();
  const auto bitMaxNum = (uint32_t)((1UL << (uint64_t)bitWidth));

  for(uint32_t bitNum=0; bitNum<bitMaxNum; bitNum++) {
    auto element = enumValues.find(bitNum);
    if(element != enumValues.end()) {
      const auto enu = element->second;
      if(enu && !enu->IsValid()) {
        continue;
      }

      if(enu) {
        const auto enumName  = enu->GetNameCalculated();
        const auto enumDescr = SvdUtils::CheckTextGeneric_SfrCC2(enu->GetDescriptionCalculated());
        const auto val       = (uint32_t)enu->GetValue().u32;

        string name = enumName;
        if(!enumDescr.empty()) {
          if(!name.empty()) {
            name += " = ";
          }
          name += enumDescr;
        }

        m_gen->Generate<CITEM  >("%s", val, name.c_str());
      }
      continue;
    }

    if(defaultValue) {
      const auto enumName  = defaultValue->GetNameCalculated();
      const auto enumDescr = SvdUtils::CheckTextGeneric_SfrCC2(defaultValue->GetDescriptionCalculated());

      auto name = enumName;
      if(!enumDescr.empty()) {
        if(!name.empty()) {
          name += " = ";
        }
        name += enumDescr;
      }

      m_gen->Generate<CITEM>("%s", bitNum, name.c_str());
    }
    else {
      m_gen->Generate<CITEM>("", bitNum);
    }
  }

  return true;
}
