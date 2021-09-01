/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteCprjModel.cpp
* @brief CMSIS RTE for *.cprj files
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteCprjModel.h"

#include "CprjFile.h"

using namespace std;

RteCprjModel::RteCprjModel(RteItem* parent) :
  RteGeneratorModel(parent)
{
}

RteCprjModel::RteCprjModel() :
  RteGeneratorModel()
{
}


RteCprjModel::~RteCprjModel()
{
  RteCprjModel::Clear();
}

void RteCprjModel::Clear()
{
  RteGeneratorModel::Clear();
}

void RteCprjModel::ClearModel()
{
  RteGeneratorModel::ClearModel();
}

CprjFile* RteCprjModel::GetCprjFile() const
{
  return dynamic_cast<CprjFile*>(m_gpdscPack);
}


bool RteCprjModel::Validate()
{
  m_bValid = RteGeneratorModel::Validate();
  CprjFile* cprjFile = GetCprjFile();
  if(!m_bValid || !cprjFile || !cprjFile->GetTargetElement()) {
    string msg = CreateErrorString("error", "R814", "cprj file is invalid or malformed");
    m_errors.push_front(msg);
    m_bValid  = false;
  }
  return m_bValid;
}


// End of RteCprjModel.cpp
