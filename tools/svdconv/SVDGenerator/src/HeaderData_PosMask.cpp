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
#include "SvdDevice.h"
#include "SvdPeripheral.h"
#include "SvdRegister.h"
#include "SvdCluster.h"
#include "SvdField.h"
#include "SvdEnum.h"
#include "SvdDimension.h"
#include "SvdTypes.h"
#include "HeaderGenAPI.h"

using namespace std;
using namespace c_header;


bool HeaderData::CreatePosMask(SvdDevice* device)
{
  CreateClustersPosMask     (device);
  CreatePeripheralsPosMask  (device);

  return true;
}

bool HeaderData::CreateClustersPosMask(SvdDevice* device)
{
  const auto& clusters = device->GetClusterList();
  if(clusters.empty()) {
    return true;
  }

  m_gen->Generate<DESCR|HEADER         >("Pos/Mask Cluster Section");
  m_gen->Generate<MAKE|MK_DOXYADDGROUP >("PosMask_clusters");
  
  for(const auto clust : clusters) {
    if(!clust || !clust->IsValid()) {
      continue;
    }

    CreateClusterPosMask(clust);
  }

  m_gen->Generate<MAKE|MK_DOXYENDGROUP >("PosMask_clusters");

  return true;
}

bool HeaderData::CreatePeripheralsPosMask(SvdDevice* device)
{
  if(!device) {
    return false;
  }

  m_gen->Generate<DESCR|HEADER         >("Pos/Mask Peripheral Section");
  m_gen->Generate<MAKE|MK_DOXYADDGROUP >("PosMask_peripherals");
  
  const auto& peris = device->GetPeripheralList();
  for(const auto peri : peris) {
    if(!peri || !peri->IsValid()) {
      continue;
    }

    CreatePeripheralPosMask(peri);
  }

  m_gen->Generate<MAKE|MK_DOXYENDGROUP >("PosMask_peripherals");

  return true;
}

bool HeaderData::CreatePeripheralPosMask(SvdPeripheral* peri)
{
  if(!peri) {
    return false;
  }

  PosMaskNames posMaskNames;
  posMaskNames.name = peri->GetHeaderTypeName();  //GetNameCalculated();

  m_gen->Generate<DESCR|PART  >("%s", posMaskNames.name.c_str());

  const auto cont = peri->GetRegisterContainer();
  if(!cont) {
    return true;
  }

  CreateRegistersPosMask(cont, &posMaskNames);

  return true;
}


bool HeaderData::CreateClusterPosMask(SvdCluster* cluster)
{
  if(!cluster) {
    return false;
  }

  PosMaskNames posMaskNames;
  posMaskNames.name = cluster->GetNameCalculated();
  
  m_gen->Generate<DESCR|PART  >("%s", posMaskNames.name.c_str());
  CreateRegistersPosMask(cluster, &posMaskNames);

  return true;
}

bool HeaderData::CreateRegistersPosMask(SvdItem* container, PosMaskNames *posMaskNames)
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
        const auto expr = dim->GetExpression();
        if(expr) {
          const auto exprType = expr->GetType();
          if(exprType == SvdTypes::Expression::EXTEND) {                 // roll out
            const auto& dimChilds = dim->GetChildren();
            for(const auto dimChild : dimChilds) {
              const auto dimReg = dynamic_cast<SvdRegister*>(dimChild);
              if(!dimReg) {
                continue;
              }

              CreateRegisterPosMask(dimReg, posMaskNames);
            }
          }
          else if(exprType == SvdTypes::Expression::ARRAY) {     // create generics
            CreateRegisterPosMask(reg, posMaskNames);
          }
        }
        else {                                                    // create generics
          CreateRegisterPosMask(reg, posMaskNames);
        }
      }
      else {                                                      // create generics
        CreateRegisterPosMask(reg, posMaskNames);
      }
    }

    const auto clust = dynamic_cast<SvdCluster*>(item);
    if(!clust || !clust->IsValid()) {
      continue;
    }
  }

  return true;
}

bool HeaderData::CreateRegisterPosMask(SvdRegister* reg, PosMaskNames *posMaskNames)
{
  if(!reg) {
    return false;
  }

  posMaskNames->reg = reg->GetHeaderFileName();
  
  m_gen->Generate<DESCR|SUBPART  >("%s", posMaskNames->reg.c_str());

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

    CreateFieldPosMask(field, posMaskNames);
  }

  return true;
}

/* Pos Mask generation
 * --------------------
 * Customer wants to partly dim registers 14,15 and 18,19 (gap).
 * dim with SameName%s and different indexes is only allowed on register level.
 * This cannot be done on higher level (cluster, peripheral) because those are generated as struct (and same names are not allowed).
 * Therefore we only expand POS/MSK for registers, but not for cluster or peripheral level.
 */
bool HeaderData::CreateFieldPosMask(SvdField* field, PosMaskNames *posMaskNames)
{
  const auto& alternateGroup  = posMaskNames->alternate;
  const auto  name            = field->GetNameCalculated(); // posMaskNames->name;
  //const auto& regOutputName   = posMaskNames->reg;
  const auto  fieldName       = field->GetHierarchicalNameResulting(); //field->GetNameCalculated();
  uint32_t firstBit           = (uint32_t)field->GetOffset();
  uint32_t bitWidth           = field->GetEffectiveBitWidth();
  uint32_t bitMaxNum          = (uint32_t) ((((uint64_t)(1) << bitWidth) -1));

  if(!alternateGroup.empty()) 
    m_gen->Generate<MAKE|MK_FIELD_POSMASK3  >("%s_%s", name.c_str(), firstBit, bitMaxNum, fieldName.c_str(), alternateGroup.c_str());
  else 
    m_gen->Generate<MAKE|MK_FIELD_POSMASK3  >("%s",    name.c_str(), firstBit, bitMaxNum, fieldName.c_str());

  return true;
}
