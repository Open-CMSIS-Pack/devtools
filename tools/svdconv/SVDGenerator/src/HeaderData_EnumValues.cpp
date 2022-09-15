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
#include "ErrLog.h"
#include "SvdDevice.h"
#include "SvdPeripheral.h"
#include "SvdRegister.h"
#include "SvdCluster.h"
#include "SvdField.h"
#include "SvdEnum.h"
#include "SvdDimension.h"
#include "SvdUtils.h"
#include "HeaderGenAPI.h"

using namespace std;
using namespace c_header;


bool HeaderData::CreateEnumValue(SvdDevice* device)
{
  CreateClustersEnumValue     (device);
  CreatePeripheralsEnumValue  (device);

  return true;
}

bool HeaderData::CreateClustersEnumValue(SvdDevice* device)
{
  const auto& clusters = device->GetClusterList();
  if(clusters.empty()) {
    return true;
  }

  m_gen->Generate<DESCR|HEADER         >("Enumerated Values Cluster Section");
  m_gen->Generate<MAKE|MK_DOXYADDGROUP >("EnumValue_clusters");
  
  for(const auto clust : clusters) {
    if(!clust || !clust->IsValid()) {
      continue;
    }

    CreateClusterEnumValue(clust);
  }

  m_gen->Generate<MAKE|MK_DOXYENDGROUP >("EnumValue_clusters");

  return true;
}

bool HeaderData::CreatePeripheralsEnumValue(SvdDevice* device)
{
  m_gen->Generate<DESCR|HEADER         >("Enumerated Values Peripheral Section");
  m_gen->Generate<MAKE|MK_DOXYADDGROUP >("EnumValue_peripherals");
  
  const auto& peris = device->GetPeripheralList();
  for(const auto peri : peris) {
    if(!peri || !peri->IsValid()) {
      continue;
    }

    CreatePeripheralEnumValue(peri);
  }

  m_gen->Generate<MAKE|MK_DOXYENDGROUP >("EnumValue_peripherals");

  return true;
}

bool HeaderData::CreatePeripheralEnumValue(SvdPeripheral* peri)
{
  EnumValuesNames enumValuesNames;
  enumValuesNames.name = peri->GetNameCalculated();

  m_gen->Generate<DESCR|PART>("%s", enumValuesNames.name.c_str());

  CreatePeripheralEnumArrayValue(peri, &enumValuesNames);

  const auto cont = peri->GetRegisterContainer();
  if(!cont) {
    return true;
  }

  CreateRegistersEnumValue(cont, &enumValuesNames);

  return true;
}

// Create enum values for enum-ing a Peripheral Array
bool HeaderData::CreatePeripheralEnumArrayValue(SvdPeripheral* peri, EnumValuesNames *enumValuesNames)
{
  const auto enumContainer = peri->GetEnumContainer();
  if(!enumContainer) {
    return true;
  }

  const auto& alternateGroup  = enumValuesNames->alternate;
  const auto& name            = enumValuesNames->name;
  const auto& regOutputName   = enumValuesNames->reg;

  string outName = name;
  outName += " ";
  outName += regOutputName;

  if(!alternateGroup.empty()) {
    outName += alternateGroup;
    outName += " ";
  }
 
  m_gen->Generate<DESCR|SUBPART  >("%s", outName.c_str());
  CreateEnumValuesContainer(enumContainer, enumValuesNames);

  return true;
}

bool HeaderData::CreateClusterEnumValue(SvdCluster* cluster)
{
  EnumValuesNames enumValuesNames;
  enumValuesNames.name = cluster->GetNameCalculated();
  
  m_gen->Generate<DESCR|PART  >("%s", enumValuesNames.name.c_str());

  CreateClusterEnumArrayValue(cluster, &enumValuesNames);
  CreateRegistersEnumValue(cluster, &enumValuesNames);

  return true;
}

// Create enum values for enum-ing a Cluster Array
bool HeaderData::CreateClusterEnumArrayValue(SvdCluster* clust, EnumValuesNames *enumValuesNames)
{
  const auto enumContainer = clust->GetEnumContainer();
  if(!enumContainer) {
    return true;
  }

  const auto& alternateGroup  = enumValuesNames->alternate;
  const auto& name            = enumValuesNames->name;
  const auto& regOutputName   = enumValuesNames->reg;

  string outName = name;
  outName += " ";
  outName += regOutputName;

  if(!alternateGroup.empty()) {
    outName += alternateGroup;
    outName += " ";
  }
 
  m_gen->Generate<DESCR|SUBPART  >("%s", outName.c_str());
  CreateEnumValuesContainer(enumContainer, enumValuesNames);

  return true;
}

bool HeaderData::CreateClusterRegistersEnumValue(SvdItem* container, EnumValuesNames *enumValuesNames)
{
  const auto& items = container->GetChildren();
  for(const auto item : items) {
    if(!item || !item->IsValid()) {
      continue;
    }

    const auto reg = dynamic_cast<SvdRegister*>(item);
    if(reg) {
      CreateRegisterEnumValue(reg, enumValuesNames);
    }

    const auto clust = dynamic_cast<SvdCluster*>(item);
    if(clust) {
      CreateClusterRegistersEnumValue(clust, enumValuesNames);
    }
  }

  return true;
}

bool HeaderData::CreateRegistersEnumValue(SvdItem* container, EnumValuesNames *enumValuesNames)
{
  const auto& childs = container->GetChildren();
  for(const auto item : childs) {
    if(!item || !item->IsValid()) {
      continue;
    }

    const auto reg = dynamic_cast<SvdRegister*>(item);
    if(reg) {
      const auto dim = reg->GetDimension();
      if(dim) {
        const auto& expr = dim->GetExpression();
        if(expr) {
          const auto exprType = expr->GetType();
          if(exprType == SvdTypes::Expression::ARRAY) {
            CreateRegisterEnumValue(reg, enumValuesNames);
            continue;
          }
          else if(exprType == SvdTypes::Expression::EXTEND) {
            CreateRegistersEnumValue(dim, enumValuesNames);
            continue;
          }
        }
      }

      CreateRegisterEnumValue(reg, enumValuesNames);
    }

    const auto clust = dynamic_cast<SvdCluster*>(item);
    if(!clust || !clust->IsValid()) {
      continue;
    }
  }

  return true;
}

bool HeaderData::CreateRegisterEnumValue(SvdRegister* reg, EnumValuesNames *enumValuesNames)
{
  enumValuesNames->reg = reg->GetHeaderFileName();
   
  m_gen->Generate<DESCR|SUBPART  >("%s", enumValuesNames->reg.c_str());

  CreateRegisterEnumArrayValue(reg, enumValuesNames);

  const auto cont = reg->GetFieldContainer();
  if(!cont) {
    return true;
  }

  const auto& childs = cont->GetChildren();
  for(const auto child : childs) {
    const auto field = dynamic_cast<SvdField*>(child);
    if(!field || !field->IsValid()) {
      continue;
    }

    CreateFieldEnumValue(field, enumValuesNames);
  }

  return true;
}

// Create enum values for enum-ing a Register Array
bool HeaderData::CreateRegisterEnumArrayValue(SvdRegister* reg, EnumValuesNames *enumValuesNames)
{
  const auto enumContainer = reg->GetEnumContainer();
  if(!enumContainer) {
    return true;
  }

  const auto& alternateGroup  = enumValuesNames->alternate;
  const auto& name            = enumValuesNames->name;
  const auto& regOutputName   = enumValuesNames->reg;

  string outName = name;
  outName += " ";
  outName += regOutputName;

  if(!alternateGroup.empty()) {
    outName += alternateGroup;
    outName += " ";
  }
 
  m_gen->Generate<DESCR|SUBPART  >("%s", outName.c_str());
  CreateEnumValuesContainer(enumContainer, enumValuesNames);

  return true;
}

bool HeaderData::CreateFieldEnumValue(SvdField* field, EnumValuesNames *enumValuesNames)
{
  const auto& enumContainers = field->GetEnumContainer();
  if(enumContainers.empty()) {
    return true;
  }

  const auto& alternateGroup = enumValuesNames->alternate;
  const auto& name           = enumValuesNames->name;
  const auto& regOutputName  = enumValuesNames->reg;

  const string fieldName = field->GetNameCalculated();
  uint32_t firstBit  = (uint32_t)field->GetOffset();
  uint32_t bitWidth  = field->GetEffectiveBitWidth();

  string outName = name;
  outName += " ";
  outName += regOutputName;
  outName += " ";
  outName += fieldName;

  if(!alternateGroup.empty()) {
    outName += alternateGroup;
    outName += " ";
  }

  outName += " [";
  outName += SvdUtils::CreateDecNum(firstBit);
  outName += "..";
  outName += SvdUtils::CreateDecNum(firstBit+bitWidth-1);
  outName += "]";
  
  m_gen->Generate<DESCR|SUBPART  >("%s", outName.c_str());
  
  for(const auto enumCont : enumContainers) {
    if(!enumCont || !enumCont->IsValid()) {
      continue;
    }

    CreateEnumValuesContainer(enumCont, enumValuesNames);
  }

  return true;
}

bool HeaderData::CreateEnumValuesContainer(SvdEnumContainer* enumCont, EnumValuesNames *enumValuesNames)
{
  const auto& childs = enumCont->GetChildren();
  if(childs.empty()) {
    return true;
  }

  const auto  containerName   = enumCont->GetHierarchicalName(); //enumCont->GetNameCalculated();
  const auto& headerEnumName  = enumCont->GetHeaderEnumName(); 
  const auto& descr           = enumCont->GetDescription();
  
  enumValuesNames->headerEnumName = headerEnumName;

  m_gen->Generate<ENUM|TYPEDEF|BEGIN    >("");
  m_gen->Generate<MAKE|MK_DOXY_COMMENT  >("%s", !descr.empty()? descr.c_str() : containerName.c_str());

  for(const auto child : childs) {
    SvdEnum* enu = dynamic_cast<SvdEnum*>(child);
    if(!enu || !enu->IsValid() || enu->IsDefault()) {
      continue;
    }

    const auto enumName = enu->GetHierarchicalName();
    const auto find = m_usedEnumValues.find(enumName);
    if(find == m_usedEnumValues.end()) {
      CreateEnumValue(enu, enumValuesNames);
      m_usedEnumValues[enumName] = enu;
    }
    else {
      const auto enuFound = find->second;
      const auto name = enu->GetNameCalculated();
      uint32_t val = enuFound->GetValue().u32;
      const auto lineNo = enuFound->GetLineNumber();
      uint32_t conflictValue = enu->GetValue().u32;
      m_gen->Generate<C_ERROR >("Enumerated Value '%s:%d' already defined as Value %d", lineNo, name.c_str(), conflictValue, val);
    }
  }
  
  string name;
  if(!headerEnumName.empty()) {
    name = headerEnumName;
  }
  else {
    name = containerName;
    const auto usage = enumCont->GetUsage();
    if(usage == SvdTypes::EnumUsage::READ) {
      name += "_R";
    }
    else if(usage == SvdTypes::EnumUsage::WRITE) {
      name += "_W";
    }
  }

  m_gen->Generate<ENUM|TYPEDEF|END  >("%s", name.c_str());
  m_gen->Generate<DIRECT  >("");

  return true;
}

bool HeaderData::CreateEnumValue(SvdEnum* enu, EnumValuesNames *enumValuesNames)
{
  const auto name = enu->GetNameCalculated();
  string enumName;
  
  if(!enumValuesNames->headerEnumName.empty()) {
    enumName  = enumValuesNames->headerEnumName;
    enumName += "_";
    enumName += name;
  }
  else {
    enumName  = enu->GetHierarchicalName();
  }

  uint32_t val = enu->GetValue().u32;
  string enumDescr = enu->GetDescriptionCalculated();
  if(enumDescr.empty()) {
    enumDescr = enumName;
  }

  m_gen->Generate<MAKE|MK_ENUMVALUE  >("%s", val, name.c_str(), enumDescr.c_str(), enumName.c_str());

  return true;
}
