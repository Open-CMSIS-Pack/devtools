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
#include "SvdCluster.h"
#include "SvdDimension.h"
#include "SvdTypes.h"
#include "HeaderGenAPI.h"

using namespace std;
using namespace c_header;


bool HeaderData::CreateClusters(SvdDevice* device)
{
  const auto& clusters = device->GetClusterList();
  if(clusters.empty()) {
    return true;
  }

  m_gen->Generate<DESCR|HEADER         >("Device Specific Cluster Section");
  m_gen->Generate<MAKE|MK_DOXYADDGROUP >("Device_Peripheral_clusters");
    
  for(const auto clust : clusters) {
    if(!clust || !clust->IsValid()) {
      continue;
    }

    CreateCluster(clust);
  }

  m_gen->Generate<MAKE|MK_DOXYENDGROUP >("Device_Peripheral_clusters");

  return true;
}

bool HeaderData::CreateCluster(SvdCluster* cluster)
{
  m_addressCnt  = 0;
  m_reservedCnt = 0;

  //CalculateMaxPaddingWidth(cluster);
  uint32_t maxWidth = cluster->GetBitWidth();
  SetMaxBitWidth(maxWidth);

  OpenCluster(cluster);
  CreateRegisters(cluster);
  CloseCluster(cluster);  

  return true;
}

bool HeaderData::OpenCluster(SvdCluster* cluster)
{
  const auto headerTypeName  = cluster->GetHeaderTypeNameHierarchical(); //GetHeaderTypeName();
  const auto clusterName     = cluster->GetNameCalculated();
  const auto descr           = cluster->GetDescriptionCalculated();
  //uint32_t baseAddress     = (uint32_t)cluster->GetAddress();

  m_reservedPad.clear();

  string text = headerTypeName;
  text += " [";
  text += clusterName;
  text += "]";
  if(!descr.empty()) {
    text += " (";
    text += descr;
    text += ")";
  }

  m_gen->Generate<MAKE|MK_DOXYADDPERI      >("%s", text.c_str());
  m_gen->Generate<STRUCT|BEGIN|TYPEDEF     >("");
  
  return true;
}

bool HeaderData::CloseCluster(SvdCluster* cluster)
{
  const auto name = cluster->GetHeaderTypeNameHierarchical(); //GetHeaderTypeName();

  uint32_t maxWidth = cluster->GetBitWidth();
  uint32_t remain = m_addressCnt % (maxWidth / 8);
  if(remain) {
    int32_t genRes = (int32_t)4 - remain;
    GenerateReserved(genRes, m_addressCnt, false);
    m_addressCnt += genRes;
  }
  
  remain = m_addressCnt % (maxWidth / 8);
  if(remain) {
    m_gen->Generate<C_ERROR >("Struct end-padding calculation error!", -1);
  }

  SvdDimension* dim = cluster->GetDimension();
  if(dim) {
    uint32_t clustSize = m_addressCnt; //cluster->GetSize();
    uint32_t clustInc  = dim->GetDimIncrement();
    
    if(clustSize <= clustInc) {
      int32_t reserved = clustInc - clustSize;
      int32_t checkRes = clustInc - m_addressCnt;

      GenerateReserved(reserved, (uint32_t)cluster->GetAbsoluteAddress() + clustSize, false);
      m_addressCnt += reserved;

      if(reserved != checkRes) {
        m_gen->Generate<C_ERROR >("Reserved bytes calculation error!", -1);
        }
    }
  }

  GenerateReserved();
  cluster->SetSize(m_addressCnt);

  m_gen->Generate<STRUCT|END|TYPEDEF >("%s", name.c_str());

  if(m_bDebugHeaderfile) {
    uint32_t size = cluster->GetSize();
    m_gen->Generate<MAKE|MK_DOXY_COMMENT  >("Size = %i (0x%x)", size, size);
  }

  m_gen->Generate<DIRECT >("");

  if(m_reservedPad.size()) {
    m_gen->Generate<C_ERROR >("Not generated remaining reserved bytes error!", -1);
  }

  return true;
}
